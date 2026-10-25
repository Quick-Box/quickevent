#include "ofeedclient.h"
#include "ofeedclientwidget.h"

#include "../../eventplugin.h"

#include <qf/gui/framework/application.h>
#include <qf/gui/framework/mainwindow.h>
#include <qf/gui/dialogs/dialog.h>
#include <qf/gui/dialogs/messagebox.h>
#include <qf/core/log.h>
#include <qf/core/utils/htmlutils.h>

#include <qf/core/sql/connection.h>
#include <qf/core/sql/query.h>
#include <qf/core/sql/qxsql.h>
#include <qf/core/sql/qxrecchng.h>
#include <qf/core/sql/transaction.h>

#include <plugins/Runs/src/runsplugin.h>
#include <plugins/Relays/src/relaysplugin.h>
#include <plugins/Competitors/src/competitordocument.h>

#include <quickevent/core/utils.h>

#include <QCoreApplication>
#include <QBuffer>
#include <QMessageBox>
#include <QHttpPart>
#include <QImageReader>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QSettings>
#include <QStandardPaths>
#include <QTextStream>
#include <QTimer>
#include <QDateTime>
#include <QTimeZone>
#include <QUrl>
#include <QUrlQuery>
#include <QDebug>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QSet>

#include <iostream>
#include <sstream>

using Event::EventPlugin;
using qf::gui::framework::getPlugin;
using Relays::RelaysPlugin;
using Runs::RunsPlugin;

namespace Event::services {

	namespace
	{
		const QString k_default_host_url = QStringLiteral("https://api.orienteerfeed.com");
		const QString k_event_config_prefix = QStringLiteral("event");
		const QString k_start_changelog_origin = QStringLiteral("START");
		const QString k_office_changelog_origin = QStringLiteral("OFFICE");
		/// window in which the record changes are collected before they are sent to OFeed
		const int k_run_change_flush_msec = 100;
		/// failed changelog entry is retried this many times, then it is left unprocessed in OFeed
		const int k_max_change_attempts = 10;

		QString normalized_base_host_url(QString host_url)
		{
			host_url = host_url.trimmed();
			if (host_url.isEmpty())
				host_url = k_default_host_url;

			if (!host_url.contains("://"))
				host_url.prepend(QStringLiteral("https://"));

			QUrl parsed_url = QUrl::fromUserInput(host_url);
			if (!parsed_url.isValid() || parsed_url.host().isEmpty())
				parsed_url = QUrl(k_default_host_url);

			QString host = parsed_url.host().toLower();
			if (host == QStringLiteral("orienteerfeed.com") || host == QStringLiteral("www.orienteerfeed.com"))
			{
				parsed_url.setHost(QStringLiteral("api.orienteerfeed.com"));
			}

			QUrl base_url;
			base_url.setScheme(parsed_url.scheme().isEmpty() ? QStringLiteral("https") : parsed_url.scheme());
			base_url.setHost(parsed_url.host());
			if (parsed_url.port() > 0)
				base_url.setPort(parsed_url.port());
			return base_url.toString();
		}

		QString normalized_receipt_host_url(QString host_url)
		{
			host_url = host_url.trimmed();
			if (host_url.isEmpty())
				host_url = QStringLiteral("https://orienteerfeed.com");

			if (!host_url.contains("://"))
				host_url.prepend(QStringLiteral("https://"));

			QUrl parsed_url = QUrl::fromUserInput(host_url);
			if (!parsed_url.isValid() || parsed_url.host().isEmpty())
				parsed_url = QUrl(QStringLiteral("https://orienteerfeed.com"));

			QString host = parsed_url.host().toLower();
			if (host == QStringLiteral("api.orienteerfeed.com") || host == QStringLiteral("www.orienteerfeed.com"))
			{
				parsed_url.setHost(QStringLiteral("orienteerfeed.com"));
			}

			QUrl base_url;
			base_url.setScheme(parsed_url.scheme().isEmpty() ? QStringLiteral("https") : parsed_url.scheme());
			base_url.setHost(parsed_url.host());
			if (parsed_url.port() > 0)
				base_url.setPort(parsed_url.port());
			return base_url.toString();
		}

	}

OFeedClient::OFeedClient(QObject *parent)
	: Super(OFeedClient::serviceName(), parent)
{
	m_networkManager = new QNetworkAccessManager(this);
	m_exportTimer = new QTimer(this);
	connect(m_exportTimer, &QTimer::timeout, this, &OFeedClient::onExportTimerTimeOut);
	m_changesTimer = new QTimer(this);
	connect(m_changesTimer, &QTimer::timeout, this, &OFeedClient::onChangesTimerTimeOut);
	m_credentialCheckTimer = new QTimer(this);
	connect(m_credentialCheckTimer, &QTimer::timeout, this, &OFeedClient::checkCredentials);
	m_runChangeFlushTimer = new QTimer(this);
	m_runChangeFlushTimer->setSingleShot(true);
	connect(m_runChangeFlushTimer, &QTimer::timeout, this, &OFeedClient::flushRunChanges);
	connect(this, &OFeedClient::settingsChanged, this, &OFeedClient::init, Qt::QueuedConnection);
	connect(qf::gui::framework::Application::instance()->qxSql(), &qf::core::sql::QxSql::recChng,
			this, &OFeedClient::onQxRecChng, Qt::QueuedConnection);
}

QString OFeedClient::serviceName()
{
	return QStringLiteral("OFeed");
}

void OFeedClient::run()
{
	if (m_startupCheckInProgress)
		return;

	const QString event_id = eventId().trimmed();
	const QString event_password = eventPassword().trimmed();
	if (event_id.isEmpty() || event_password.isEmpty()) {
		setCredentialsInvalid();
		showStartupCredentialError(tr("OFeed event ID or password is not set."));
		return;
	}

	m_startupCheckInProgress = true;
	testConnection(hostUrl(), event_id, event_password, [this](bool success, const QString &message) {
		m_startupCheckInProgress = false;
		if (!success) {
			setCredentialsInvalid();
			showStartupCredentialError(message);
			return;
		}
		m_credentialsValid = 1;
		m_credentialWarningShown = false;
		emit credentialsStatusChanged(true);
		startService();
	});
}

void OFeedClient::startService()
{
	Super::run();
	ensureEventImageCachedAtStartup();
	exportStartListAndResults();
	m_exportTimer->start();
	m_changesTimer->start();
	m_credentialCheckTimer->start();
}

void OFeedClient::setCredentialsInvalid()
{
	m_credentialsValid = 0;
	emit credentialsStatusChanged(false);
}

void OFeedClient::showStartupCredentialError(const QString &message)
{
	QMessageBox::warning(
		qf::gui::framework::MainWindow::frameWork(),
		serviceDisplayName(),
		tr("The service cannot be started, OFeed credentials check has not passed.\n\n%1\n\nPlease open OFeed service settings and update your credentials.").arg(message)
	);
}

void OFeedClient::setRunning(bool on)
{
	const bool start_changes_off = !runStartChangesProcessing();
	const bool office_changes_off = !runOfficeChangesProcessing();
	if (on && !isRunning() && (start_changes_off || office_changes_off)) {
		QString text;
		QString informative_text;
		if (start_changes_off && office_changes_off) {
			text = tr("Do you really want to start the service without processing any changes from OFeed?");
			informative_text = tr("Neither start changes from O Checklist nor late entries and changes from the office will be processed."
								  " Choose No to switch both of them on before the service starts.");
		}
		else if (start_changes_off) {
			text = tr("Do you really want to start the service without processing start changes from O Checklist?");
			informative_text = tr("Late entries and changes from the office will be processed."
								  " Choose No to switch the start changes processing on as well.");
		}
		else {
			text = tr("Do you really want to start the service without processing late entries and changes from the office?");
			informative_text = tr("Start changes from O Checklist will be processed."
								  " Choose No to switch the office changes processing on as well.");
		}
		qf::gui::dialogs::MessageBox mbx(qf::gui::framework::MainWindow::frameWork());
		mbx.setIcon(QMessageBox::Question);
		mbx.setWindowTitle(serviceDisplayName());
		mbx.setText(text);
		mbx.setInformativeText(informative_text);
		mbx.setStandardButtons(QMessageBox::Yes | QMessageBox::No);
		mbx.setDefaultButton(QMessageBox::No);
		mbx.setDoNotShowAgainPersistentKey(QStringLiteral("ofeed/startWithoutChangesProcessing"));
		if (mbx.exec() == QMessageBox::No) {
			if (start_changes_off) {
				setRunStartChangesProcessing(true);
			}
			if (office_changes_off) {
				setRunOfficeChangesProcessing(true);
			}
		}
	}
	Super::setRunning(on);
}

void OFeedClient::stop()
{
	Super::stop();
	m_exportTimer->stop();
	m_changesTimer->stop();
	m_credentialCheckTimer->stop();
	m_exportDeferred = false;
	m_credentialsValid = -1;
	m_credentialWarningShown = false;
	m_resultsExportInProgress = false;
	m_startListExportInProgress = false;
	m_changesProcessingInProgress = false;
	m_processingOFeedChanges = false;
	m_runChangeFlushTimer->stop();
	m_pendingRunChanges.clear();
	m_pendingNewRuns.clear();
}

void OFeedClient::exportResultsIofXml3()
{
	if (m_resultsExportInProgress) {
		qfDebug() << serviceName() << "results upload already in progress, skipping";
		return;
	}
	m_resultsExportInProgress = true;

	int current_stage = getPlugin<EventPlugin>()->currentStageId();
	bool is_relays = getPlugin<EventPlugin>()->eventConfig().isRelays();

	QString str = is_relays
					  ? getPlugin<RelaysPlugin>()->resultsIofXml30()
					  : getPlugin<RunsPlugin>()->resultsIofXml30Stage(current_stage);

	sendFile(tr("results upload"), "/rest/v1/upload/iof", str, nullptr, [this]() {
		m_resultsExportInProgress = false;
	});
}

void OFeedClient::exportStartListIofXml3(std::function<void()> on_success)
{
	if (m_startListExportInProgress) {
		qfDebug() << serviceName() << "start list upload already in progress, skipping";
		return;
	}
	m_startListExportInProgress = true;

	int current_stage = getPlugin<EventPlugin>()->currentStageId();
	bool is_relays = getPlugin<EventPlugin>()->eventConfig().isRelays();

	QString str = is_relays
					  ? getPlugin<RelaysPlugin>()->startListIofXml30()
					  : getPlugin<RunsPlugin>()->startListStageIofXml30(current_stage, quickevent::gui::ReportOptionsDialog::VacantsOption::AllVacants);

	// OFeed creates the competitors from the start list, except those without the required start
	// time, those stay pending until the time is set
	QSet<int> uploaded_runs;
	for (int run_id : std::as_const(m_pendingNewRuns)) {
		if (canCreateCompetitor(run_id)) {
			uploaded_runs.insert(run_id);
		}
	}

	sendFile(tr("start list upload"), "/rest/v1/upload/iof", str, [this, uploaded_runs, on_success]() {
		m_pendingNewRuns.subtract(uploaded_runs);
		if (on_success) {
			on_success();
		}
	}, [this]() {
		m_startListExportInProgress = false;
	});
}

void OFeedClient::processChanges(std::function<void()> on_done)
{
	auto process_office = [this, on_done]() {
		if (runOfficeChangesProcessing()) {
			getChangesByOrigin(k_office_changelog_origin, on_done);
		}
		else if (on_done) {
			on_done();
		}
	};
	if (runStartChangesProcessing()) {
		getChangesByOrigin(k_start_changelog_origin, process_office);
	}
	else {
		process_office();
	}
}

int OFeedClient::credentialCheckRemainingMs() const
{
	return m_credentialCheckTimer->remainingTime();
}

int OFeedClient::credentialCheckIntervalMs() const
{
	return m_credentialCheckTimer->interval();
}

int OFeedClient::exportTimerRemainingMs() const
{
	return m_exportTimer->remainingTime();
}

int OFeedClient::exportTimerIntervalMs() const
{
	return m_exportTimer->interval();
}

int OFeedClient::changesTimerRemainingMs() const
{
	return m_changesTimer->remainingTime();
}

int OFeedClient::changesTimerIntervalMs() const
{
	return m_changesTimer->interval();
}

qf::gui::framework::DialogWidget *OFeedClient::createDetailWidget()
{
	auto *w = new OFeedClientWidget();
	return w;
}

void OFeedClient::init()
{
	OFeedClientSettings ss = settings();
	m_exportTimer->setInterval(ss.exportIntervalSec() * 1000);
	m_changesTimer->setInterval(ss.changesIntervalSec() * 1000);
	m_credentialCheckTimer->setInterval(ss.credentialCheckIntervalMin() * 60 * 1000);
	ensureEventImageCachedAtStartup();
}

void OFeedClient::exportStartListAndResults(bool only_enabled)
{
	const bool start_list = !only_enabled || uploadStartList();
	const bool results = !only_enabled || uploadResults();
	if (start_list) {
		exportStartListIofXml3(results ? [this]() { exportResultsIofXml3(); } : std::function<void()>());
	}
	else if (results) {
		exportResultsIofXml3();
	}
}

void OFeedClient::onExportTimerTimeOut()
{
	emit exportTimerFired();
	if (m_changesProcessingInProgress) {
		// Exporting now would send stale data and overwrite OFeed changes, the running
		// changes cycle triggers the deferred export as soon as it is done.
		m_exportDeferred = true;
		return;
	}
	exportStartListAndResults(true);
}

void OFeedClient::onChangesTimerTimeOut()
{
	emit changesTimerFired();
	if (!runStartChangesProcessing() && !runOfficeChangesProcessing()) {
		runDeferredExport();
		return;
	}
	processChanges([this]() { runDeferredExport(); });
}

void OFeedClient::runDeferredExport()
{
	if (!m_exportDeferred) {
		return;
	}
	m_exportDeferred = false;
	exportStartListAndResults(true);
}

void OFeedClient::loadSettings()
{
	Super::loadSettings();
	init();
}

void OFeedClient::onQxRecChng(const qf::core::sql::QxRecChng &recchng, QObject *source)
{
	if (status() != Status::Running)
		return;
	if (source == this) {
		// own change, do not send it back to OFeed
		return;
	}

	int stage_id = getPlugin<EventPlugin>()->currentStageId();

	if (recchng.table == QLatin1String("runs")) {
		int run_id = static_cast<int>(recchng.id);
		switch (recchng.op) {
		case qf::core::sql::RecOp::Insert: {
			// runs record is inserted for every stage, the new competitor is sent just once
			if (getPlugin<EventPlugin>()->stageIdForRun(run_id) != stage_id) {
				return;
			}
			if (m_runsInsertedFromOFeed.remove(run_id)) {
				qfInfo() << serviceName().toStdString() + " [new competitor]: added from OFeed, no need to send back as a new competitor from QE (already exists in OFeed)";
				return;
			}
			qfInfo() << serviceName().toStdString() + " rec chng competitor ADDED, run id: " << run_id;
			createCompetitorOrDefer(run_id);
			return;
		}
		case qf::core::sql::RecOp::Delete: {
			// WARNING: the record is already gone, so its stage cannot be checked here. In a multi stage
			// event this sends a delete request for the run of every stage, while OFeed knows the
			// current stage run id only and answers with an error for the rest of them.
			m_pendingRunChanges.remove(run_id);
			if (m_pendingNewRuns.remove(run_id)) {
				// never created in OFeed
				return;
			}
			qfInfo() << serviceName().toStdString() + " rec chng competitor DELETED, run id: " << run_id;
			sendCompetitorDeleted(run_id);
			return;
		}
		case qf::core::sql::RecOp::Update:
			queueRunChange(run_id, recchng.table, recchng.record);
			return;
		}
	}
	else if (recchng.table == QLatin1String("competitors") && recchng.op == qf::core::sql::RecOp::Update) {
		int run_id = getPlugin<RunsPlugin>()->runForCompetitorStage(static_cast<int>(recchng.id), stage_id);
		if (run_id > 0) {
			queueRunChange(run_id, recchng.table, recchng.record);
		}
	}
}

void OFeedClient::queueRunChange(int run_id, const QString &table, const QVariantMap &fields)
{
	if (m_processingOFeedChanges) {
		qfDebug() << serviceName() << "skipping run change back-send to OFeed (change originated from OFeed)";
		return;
	}
	auto &pending = m_pendingRunChanges[run_id];
	auto &dest = (table == QLatin1String("runs"))? pending.runFields: pending.competitorFields;
	// field names are normalized here, so that the rest of the code can rely on lowercase
	// names without the table prefix, the record changes come in both flavors
	for (const auto &[key, val] : fields.asKeyValueRange()) {
		dest[key.section('.', -1).toLower()] = val;
	}
	// collect the changes of one record into a single request, a card read-out updates the run
	// record twice - the checked card values first, the computed running time after that
	if (!m_runChangeFlushTimer->isActive()) {
		m_runChangeFlushTimer->start(k_run_change_flush_msec);
	}
}

/// One-time upload of the rental cards table (lentcards) to OFeed, "ignored" cards are uploaded as not active
/// @param callback success, number of the cards stored by OFeed, error message
void OFeedClient::uploadRentalCards(std::function<void(bool success, int count, const QString &error)> callback)
{
	QByteArray csv = "cardNumber,active\n";
	int count = 0;
	try {
		qf::core::sql::Query q;
		q.exec("SELECT siId, ignored FROM lentcards ORDER BY siId", qf::core::Exception::Throw);
		while (q.next()) {
			csv += QByteArray::number(q.value(0).toInt()) + (q.value(1).toBool()? ",false\n": ",true\n");
			++count;
		}
	}
	catch (const std::exception &e) {
		callback(false, 0, tr("Reading of the rental cards failed: %1").arg(QString::fromUtf8(e.what())));
		return;
	}
	if (count == 0) {
		callback(true, 0, QString());
		return;
	}
	// the limit of the OFeed import
	if (csv.size() > 200000) {
		callback(false, 0, tr("There are too many rental cards, OFeed accepts the file up to 200 kB."));
		return;
	}

	auto *multi_part = new QHttpMultiPart(QHttpMultiPart::FormDataType);
	QHttpPart file_part;
	file_part.setHeader(QNetworkRequest::ContentTypeHeader, QVariant("text/csv"));
	file_part.setHeader(QNetworkRequest::ContentDispositionHeader, QVariant(R"(form-data; name="file"; filename="rental-cards.csv")"));
	file_part.setBody(csv);
	multi_part->append(file_part);

	QNetworkRequest request(QUrl(hostUrl() + "/rest/v1/events/" + eventId() + "/rental-cards/import"));
	request.setRawHeader("Authorization", "Basic " + (eventId() + ":" + eventPassword()).toUtf8().toBase64());
	QNetworkReply *reply = m_networkManager->post(request, multi_part);
	multi_part->setParent(reply);

	connect(reply, &QNetworkReply::finished, this, [reply, count, callback]() {
		const QByteArray body = reply->readAll();
		const QJsonObject json = QJsonDocument::fromJson(body).object();
		if (reply->error()) {
			QString msg = json.value("message").toString();
			if (msg.isEmpty())
				msg = reply->errorString();
			qfError() << serviceName().toStdString() + " [rental cards import]: " << msg << "HTTP status:" << reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
			callback(false, 0, msg);
		}
		else {
			const int imported = json.value("results").toObject().value("imported").toInt(count);
			qfInfo() << serviceName().toStdString() + " [rental cards import]: uploaded" << imported << "cards";
			callback(true, imported, QString());
		}
		reply->deleteLater();
	});
}

/// OFeed marks the rental card as returned, only the cards of the rental cards table are known there
void OFeedClient::sendRentalCardReturned(int run_id, bool returned)
{
	int card = 0;
	try {
		qf::core::sql::Query q;
		q.prepare("SELECT runs.siId FROM runs JOIN lentcards ON lentcards.siId = runs.siId WHERE runs.id = :runId", qf::core::Exception::Throw);
		q.bindValue(":runId", run_id);
		if (q.exec(qf::core::Exception::Throw) && q.next())
			card = q.value(0).toInt();
	}
	catch (const std::exception &e) {
		qfError() << tr("Lookup of the rental card failed: ") << e.what();
		return;
	}
	if (card <= 0)
		return;

	QNetworkRequest request(QUrl(hostUrl() + "/rest/v1/events/" + eventId() + "/rental-cards/" + QString::number(card) + "/returned"));
	request.setRawHeader("Authorization", "Basic " + (eventId() + ":" + eventPassword()).toUtf8().toBase64());
	request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
	QNetworkReply *reply = m_networkManager->sendCustomRequest(request, "PATCH", returned? QByteArray(R"({"returned":true})"): QByteArray(R"({"returned":false})"));
	connect(reply, &QNetworkReply::finished, this, [reply, card]() {
		if (reply->error()) {
			const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
			const QString msg = serviceName() + " [rental card returned]: card " + QString::number(card) + " " + reply->errorString() + " HTTP status: " + QString::number(status);
			// 404 - the card was not uploaded to OFeed
			if (status == 404)
				qfInfo() << msg;
			else
				qfError() << msg;
		}
		else {
			qfInfo() << serviceName().toStdString() + " [rental card returned]: card" << card << "updated";
		}
		reply->deleteLater();
	});
}

void OFeedClient::flushRunChanges()
{
	const auto pending = m_pendingRunChanges;
	m_pendingRunChanges.clear();
	for (const auto &[run_id, chng] : pending.asKeyValueRange()) {
		const QVariant card_returned = chng.runFields.value(QStringLiteral("cardreturned"));
		if (card_returned.isValid())
			sendRentalCardReturned(run_id, card_returned.toBool());
		if (m_pendingNewRuns.contains(run_id)) {
			// not in OFeed yet, the create request carries the current data of the run
			createCompetitorOrDefer(run_id);
			continue;
		}
		onRunChanged(run_id, chng.runFields, chng.competitorFields);
	}
}

QString OFeedClient::hostUrl() const
{
	int current_stage = getPlugin<EventPlugin>()->currentStageId();
	OFeedConfig cfg = getPlugin<EventPlugin>()->appDbConfig().ofeedConfig(current_stage);
	return normalized_base_host_url(cfg.hostUrl);
}

QString OFeedClient::eventId() const
{
	int current_stage = getPlugin<EventPlugin>()->currentStageId();
	OFeedConfig cfg = getPlugin<EventPlugin>()->appDbConfig().ofeedConfig(current_stage);
	return cfg.eventId;
}

QString OFeedClient::eventPassword() const
{
	int current_stage = getPlugin<EventPlugin>()->currentStageId();
	OFeedConfig cfg = getPlugin<EventPlugin>()->appDbConfig().ofeedConfig(current_stage);
	return cfg.eventPassword;
}

QDateTime OFeedClient::lastChangelogCall(const QString &origin) {
    int current_stage = getPlugin<EventPlugin>()->currentStageId();
    auto config = getPlugin<EventPlugin>()->appDbConfig().ofeedConfig(current_stage);
    const bool is_office = origin == k_office_changelog_origin;
    // Retrieve the stored value from the configuration, each origin is tracked separately
    auto &last_call = is_office ? config.lastOfficeChangelogCall : config.lastStartChangelogCall;

    // Check if the value exists
    if (!last_call.isValid() || last_call.toString().isEmpty()) {
        // No valid value exists, set the initial value
        last_call = QDateTime::fromSecsSinceEpoch(0); // Default to Unix epoch (1970-01-01T00:00:00Z)
        getPlugin<EventPlugin>()->appDbConfig().setOfeedConfig(current_stage, config);
        // qDebug() << "No lastChangelogCall found. Setting initial value to:" << last_call.toString(Qt::ISODate);
    }
    return last_call;
}

bool OFeedClient::runXmlValidation()
{
    auto &config = getPlugin<EventPlugin>()->appDbConfig();
    auto current_stage = config.eventConfig().currentStageId;
    return config.ofeedConfig(current_stage).runXmlValidation;
}

bool OFeedClient::runStartChangesProcessing ()
{
    auto &config = getPlugin<EventPlugin>()->appDbConfig();
    auto current_stage = config.eventConfig().currentStageId;
    return config.ofeedConfig(current_stage).runStartChangesProcessing;
}

bool OFeedClient::runOfficeChangesProcessing ()
{
    auto &config = getPlugin<EventPlugin>()->appDbConfig();
    auto current_stage = config.eventConfig().currentStageId;
    return config.ofeedConfig(current_stage).runOfficeChangesProcessing;
}

// QString OFeedClient::receiptConfigKey(const QString &suffix) const
// {
// 	const int current_stage = getPlugin<EventPlugin>()->currentStageId();
// 	return stageConfigKey(k_event_config_prefix, suffix, current_stage);
// }

// QVariant OFeedClient::receiptConfigValue(const QString &suffix, const QVariant &default_value) const
// {
// 	return getPlugin<EventPlugin>()->appDbConfig().value(receiptConfigKey(suffix), default_value);
// }

// void OFeedClient::setReceiptConfigValue(const QString &suffix, const QVariant &value)
// {
// 	auto *event_config = getPlugin<EventPlugin>()->appDbConfig();
// 	event_config.setValue(receiptConfigKey(suffix), value);
// 	event_config.save(k_event_config_prefix);
// }

bool OFeedClient::printEventImageOnReceipt() const
{
    auto &config = getPlugin<EventPlugin>()->appDbConfig();
    auto current_stage = config.eventConfig().currentStageId;
    return config.receiptsConfig(current_stage).printImage;
}

void OFeedClient::setPrintEventImageOnReceipt(bool on)
{
    auto &config = getPlugin<EventPlugin>()->appDbConfig();
    auto current_stage = config.eventConfig().currentStageId;
    auto print_config = config.receiptsConfig(current_stage);
    print_config.printImage = on;
    config.setReceiptsConfig(current_stage, print_config);
}

bool OFeedClient::printEventQrCodeOnReceipt() const
{
    auto &config = getPlugin<EventPlugin>()->appDbConfig();
    auto current_stage = config.eventConfig().currentStageId;
    return config.receiptsConfig(current_stage).printQrCode;
}

void OFeedClient::setPrintEventQrCodeOnReceipt(bool on)
{
    auto &config = getPlugin<EventPlugin>()->appDbConfig();
    auto current_stage = config.eventConfig().currentStageId;
    auto print_config = config.receiptsConfig(current_stage);
    print_config.printQrCode = on;
    config.setReceiptsConfig(current_stage, print_config);
}

int OFeedClient::receiptImageHeightMm() const
{
	auto &config = getPlugin<EventPlugin>()->appDbConfig();
	auto current_stage = config.eventConfig().currentStageId;
	return config.receiptsConfig(current_stage).imageHeightMm;
}

void OFeedClient::setReceiptImageHeightMm(int height_mm)
{
	if(height_mm < 10)
		height_mm = 10;
	else if(height_mm > 60)
		height_mm = 60;
	auto &config = getPlugin<EventPlugin>()->appDbConfig();
	auto current_stage = config.eventConfig().currentStageId;
	auto rc = config.receiptsConfig(current_stage);
	rc.imageHeightMm = height_mm;
	config.setReceiptsConfig(current_stage, rc);
}

QString OFeedClient::receiptEventLinkUrl() const
{
	auto &config = getPlugin<EventPlugin>()->appDbConfig();
	auto current_stage = config.eventConfig().currentStageId;
	const QString custom_url = config.receiptsConfig(current_stage).linkUrl;
	if(!custom_url.isEmpty())
		return custom_url;
	return defaultReceiptEventLinkUrl();
}

QString OFeedClient::defaultReceiptEventLinkUrl() const
{
	QUrl event_url(normalized_receipt_host_url(hostUrl()));
	const QString event_id = eventId().trimmed();
	if(event_id.isEmpty())
		return event_url.toString();
	event_url.setPath(QStringLiteral("/events/%1").arg(event_id));
	QUrlQuery query(event_url);
	query.addQueryItem(QStringLiteral("tab"), QStringLiteral("results"));
	event_url.setQuery(query);
	return event_url.toString();
}

void OFeedClient::setReceiptEventLinkUrl(QString link_url)
{
	link_url = link_url.trimmed();
	auto &config = getPlugin<EventPlugin>()->appDbConfig();
	auto current_stage = config.eventConfig().currentStageId;
	auto rc = config.receiptsConfig(current_stage);
	rc.linkUrl = link_url;
	config.setReceiptsConfig(current_stage, rc);
}

QString OFeedClient::receiptEventQrCodeCaption() const
{
	auto &config = getPlugin<EventPlugin>()->appDbConfig();
	auto current_stage = config.eventConfig().currentStageId;
	return config.receiptsConfig(current_stage).qrCodeCaption;
}

QString OFeedClient::defaultReceiptEventQrCodeCaption() const
{
	return QStringLiteral("Live Results");
}

void OFeedClient::setReceiptEventQrCodeCaption(QString caption)
{
	caption = caption.trimmed();
	auto &config = getPlugin<EventPlugin>()->appDbConfig();
	auto current_stage = config.eventConfig().currentStageId;
	auto rc = config.receiptsConfig(current_stage);
	rc.qrCodeCaption = caption;
	config.setReceiptsConfig(current_stage, rc);
}

bool OFeedClient::hasCachedEventImage() const
{
	return !cachedEventImageBase64().isEmpty();
}

QString OFeedClient::cachedEventImageBase64() const
{
	auto &config = getPlugin<EventPlugin>()->appDbConfig();
	auto current_stage = config.eventConfig().currentStageId;
	return config.receiptsConfig(current_stage).imageBase64;
}

QString OFeedClient::cachedEventImageFormat() const
{
	auto &config = getPlugin<EventPlugin>()->appDbConfig();
	auto current_stage = config.eventConfig().currentStageId;
	return config.receiptsConfig(current_stage).imageFormat;
}

void OFeedClient::setCachedEventImage(const QByteArray &raw_data, const QString &format)
{
	auto &config = getPlugin<EventPlugin>()->appDbConfig();
	auto current_stage = config.eventConfig().currentStageId;
	auto rc = config.receiptsConfig(current_stage);
	rc.imageBase64 = QString::fromLatin1(raw_data.toBase64());
	rc.imageFormat = format.toLower();
	rc.imageHeightMm = Receipts::ReceiptsConfig::imageHeightMmForImage(raw_data, rc.imageHeightMm);
	config.setReceiptsConfig(current_stage, rc);
}

void OFeedClient::clearCachedEventImage()
{
	auto &config = getPlugin<EventPlugin>()->appDbConfig();
	auto current_stage = config.eventConfig().currentStageId;
	auto rc = config.receiptsConfig(current_stage);
	rc.imageBase64.clear();
	rc.imageFormat.clear();
	config.setReceiptsConfig(current_stage, rc);
}

void OFeedClient::ensureEventImageCachedAtStartup()
{
	if(m_eventImageStartupAttempted)
		return;
	if(!printEventImageOnReceipt())
		return;
	if(hasCachedEventImage())
		return;
	m_eventImageStartupAttempted = true;
	refreshEventImageCache();
}

void OFeedClient::refreshEventImageCache(std::function<void(bool, const QString &)> callback)
{
	const QString trimmed_event_id = eventId().trimmed();
	const QString trimmed_event_password = eventPassword().trimmed();
	if(trimmed_event_id.isEmpty() || trimmed_event_password.isEmpty()) {
		if(callback)
			callback(false, tr("Missing OFeed event credentials."));
		return;
	}

	QUrl base_url(hostUrl());
	if(!base_url.isValid() || base_url.host().isEmpty()) {
		if(callback)
			callback(false, tr("Invalid OFeed URL."));
		return;
	}
	base_url.setPath(QStringLiteral("/rest/v1/events/%1/image").arg(trimmed_event_id));

	QNetworkRequest request(base_url);
	const QString combined = trimmed_event_id + ":" + trimmed_event_password;
	const QByteArray auth = "Basic " + combined.toUtf8().toBase64();
	request.setRawHeader("Authorization", auth);

	QNetworkReply *reply = m_networkManager->get(request);
	connect(reply, &QNetworkReply::finished, this, [this, reply, callback]() {
		const int http_status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
		const QByteArray payload = reply->readAll();
		const QString content_type = reply->header(QNetworkRequest::ContentTypeHeader).toString().toLower();

		auto callback_result = [&](bool success, const QString &message) {
			if(callback)
				callback(success, message);
		};

		if(reply->error() != QNetworkReply::NoError) {
			if(http_status == 404 || http_status == 204)
				callback_result(false, tr("No event image is available in OFeed."));
			else
				callback_result(false, tr("Event image download failed: %1").arg(reply->errorString()));
			reply->deleteLater();
			return;
		}
		if(http_status != 200 || payload.isEmpty()) {
			callback_result(false, tr("No event image payload received from OFeed."));
			reply->deleteLater();
			return;
		}

		const bool is_svg = content_type.contains("image/svg")
			|| payload.startsWith("<svg")
			|| payload.contains("<svg");
		if(is_svg) {
			setCachedEventImage(payload, "svg");
			callback_result(true, tr("Event image cached as SVG."));
			reply->deleteLater();
			return;
		}

		QBuffer buffer;
		buffer.setData(payload);
		buffer.open(QIODevice::ReadOnly);
		QImageReader reader(&buffer);
		reader.setAutoTransform(true);
		const QSize original_size = reader.size();
		const int max_dimension = 1400;
		if(original_size.isValid() && (original_size.width() > max_dimension || original_size.height() > max_dimension)) {
			QSize scaled = original_size;
			scaled.scale(max_dimension, max_dimension, Qt::KeepAspectRatio);
			reader.setScaledSize(scaled);
		}
		QImage image = reader.read();
		if(image.isNull()) {
			callback_result(false, tr("Unsupported image format received from OFeed."));
			reply->deleteLater();
			return;
		}
		if(image.width() > max_dimension || image.height() > max_dimension) {
			image = image.scaled(max_dimension, max_dimension, Qt::KeepAspectRatio, Qt::SmoothTransformation);
		}
		QByteArray png_data;
		{
			QBuffer out_buffer(&png_data);
			out_buffer.open(QIODevice::WriteOnly);
			if(!image.save(&out_buffer, "PNG")) {
				callback_result(false, tr("Cannot encode cached event image."));
				reply->deleteLater();
				return;
			}
		}
		if(png_data.isEmpty()) {
			callback_result(false, tr("Cached image encoding produced empty payload."));
			reply->deleteLater();
			return;
		}
		setCachedEventImage(png_data, "png");
		callback_result(true, tr("Event image cached (%1x%2).").arg(image.width()).arg(image.height()));
		reply->deleteLater();
	});
}

void OFeedClient::setHostUrl(QString hostUrl)
{
	auto &config = getPlugin<EventPlugin>()->appDbConfig();
	const int current_stage = getPlugin<EventPlugin>()->currentStageId();
	auto cfg = config.ofeedConfig(current_stage);
	cfg.hostUrl = hostUrl;
	config.setOfeedConfig(current_stage, cfg);
	m_eventImageStartupAttempted = false;
}

void OFeedClient::setEventId(QString eventId)
{
	auto &config = getPlugin<EventPlugin>()->appDbConfig();
	const int current_stage = getPlugin<EventPlugin>()->currentStageId();
	auto cfg = config.ofeedConfig(current_stage);
	cfg.eventId = eventId;
	config.setOfeedConfig(current_stage, cfg);
	m_eventImageStartupAttempted = false;
}

void OFeedClient::setEventPassword(QString eventPassword)
{
	auto &config = getPlugin<EventPlugin>()->appDbConfig();
	const int current_stage = getPlugin<EventPlugin>()->currentStageId();
	auto cfg = config.ofeedConfig(current_stage);
	cfg.eventPassword = eventPassword;
	config.setOfeedConfig(current_stage, cfg);
	m_eventImageStartupAttempted = false;
}

void OFeedClient::setLastChangelogCall(const QString &origin, QDateTime lastChangelogCall)
{
	auto &config = getPlugin<EventPlugin>()->appDbConfig();
	const int current_stage = getPlugin<EventPlugin>()->currentStageId();
	auto cfg = config.ofeedConfig(current_stage);
	if (origin == k_office_changelog_origin) {
		cfg.lastOfficeChangelogCall = lastChangelogCall;
	}
	else {
		cfg.lastStartChangelogCall = lastChangelogCall;
	}
	config.setOfeedConfig(current_stage, cfg);
}

void OFeedClient::setRunXmlValidation(bool runXmlValidation)
{
	auto &config = getPlugin<EventPlugin>()->appDbConfig();
	const int current_stage = getPlugin<EventPlugin>()->currentStageId();
	auto cfg = config.ofeedConfig(current_stage);
	cfg.runXmlValidation = runXmlValidation;
	config.setOfeedConfig(current_stage, cfg);
}

void OFeedClient::setRunStartChangesProcessing(bool runStartChangesProcessing)
{
	auto &config = getPlugin<EventPlugin>()->appDbConfig();
	const int current_stage = getPlugin<EventPlugin>()->currentStageId();
	auto cfg = config.ofeedConfig(current_stage);
	cfg.runStartChangesProcessing = runStartChangesProcessing;
	config.setOfeedConfig(current_stage, cfg);
}

void OFeedClient::setRunOfficeChangesProcessing(bool runOfficeChangesProcessing)
{
	auto &config = getPlugin<EventPlugin>()->appDbConfig();
	const int current_stage = getPlugin<EventPlugin>()->currentStageId();
	auto cfg = config.ofeedConfig(current_stage);
	cfg.runOfficeChangesProcessing = runOfficeChangesProcessing;
	config.setOfeedConfig(current_stage, cfg);
}

bool OFeedClient::uploadStartList()
{
	auto &config = getPlugin<EventPlugin>()->appDbConfig();
	auto current_stage = config.eventConfig().currentStageId;
	return config.ofeedConfig(current_stage).uploadStartList;
}

void OFeedClient::setUploadStartList(bool on)
{
	auto &config = getPlugin<EventPlugin>()->appDbConfig();
	const int current_stage = getPlugin<EventPlugin>()->currentStageId();
	auto cfg = config.ofeedConfig(current_stage);
	cfg.uploadStartList = on;
	config.setOfeedConfig(current_stage, cfg);
}

bool OFeedClient::uploadResults()
{
	auto &config = getPlugin<EventPlugin>()->appDbConfig();
	auto current_stage = config.eventConfig().currentStageId;
	return config.ofeedConfig(current_stage).uploadResults;
}

void OFeedClient::setUploadResults(bool on)
{
	auto &config = getPlugin<EventPlugin>()->appDbConfig();
	const int current_stage = getPlugin<EventPlugin>()->currentStageId();
	auto cfg = config.ofeedConfig(current_stage);
	cfg.uploadResults = on;
	config.setOfeedConfig(current_stage, cfg);
}

bool OFeedClient::introTourShowed() const
{
	auto &config = getPlugin<EventPlugin>()->appDbConfig();
	const int current_stage = getPlugin<EventPlugin>()->currentStageId();
	return config.ofeedConfig(current_stage).introTourShowed;
}

void OFeedClient::setIntroTourShowed(bool shown)
{
	auto &config = getPlugin<EventPlugin>()->appDbConfig();
	const int current_stage = getPlugin<EventPlugin>()->currentStageId();
	auto cfg = config.ofeedConfig(current_stage);
	cfg.introTourShowed = shown;
	config.setOfeedConfig(current_stage, cfg);
}

void OFeedClient::testConnection(const QString &host_url,
								 const QString &event_id,
								 const QString &event_password,
								 std::function<void(bool, const QString &)> callback)
{
	const QString trimmed_host_url = host_url.trimmed();
	const QString trimmed_event_id = event_id.trimmed();
	const QString trimmed_event_password = event_password.trimmed();

	if (trimmed_host_url.isEmpty() || trimmed_event_id.isEmpty() || trimmed_event_password.isEmpty()) {
		callback(false, tr("Please fill URL, event id, and password."));
		return;
	}

	QUrl url(normalized_base_host_url(trimmed_host_url));
	if (!url.isValid() || url.scheme().isEmpty() || url.host().isEmpty()) {
		callback(false, tr("Invalid OFeed URL."));
		return;
	}

	url.setPath(QStringLiteral("/rest/v1/events/%1/connection-check").arg(trimmed_event_id));

	QNetworkRequest request(url);
	request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
	const QString combined = trimmed_event_id + ":" + trimmed_event_password;
	request.setRawHeader("Authorization", "Basic " + combined.toUtf8().toBase64());

	QNetworkReply *reply = m_networkManager->post(request, QByteArray("{}"));

	connect(reply, &QNetworkReply::finished, this, [reply, callback]() {
		const int http_status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
		const QString http_reason = reply->attribute(QNetworkRequest::HttpReasonPhraseAttribute).toString();
		const QByteArray response_bytes = reply->readAll();
		const QString response_text = QString::fromUtf8(response_bytes).trimmed();

		auto callback_error = [http_status, response_text, callback](const QString &msg) {
			const QString status_text = http_status > 0 ? QStringLiteral("HTTP %1").arg(http_status) : QStringLiteral("HTTP status unavailable");
			const QString full_message = response_text.isEmpty()
											 ? QStringLiteral("%1: %2").arg(status_text, msg)
											 : QStringLiteral("%1: %2 | %3").arg(status_text, msg, response_text);
			callback(false, full_message);
		};

		if (reply->error() != QNetworkReply::NoError) {
			callback_error(reply->errorString());
			reply->deleteLater();
			return;
		}

		if (http_status != 200) {
			callback_error(http_reason.isEmpty() ? tr("Unexpected HTTP response") : http_reason);
			reply->deleteLater();
			return;
		}

		QJsonParseError parse_error;
		const QJsonDocument doc = QJsonDocument::fromJson(response_bytes, &parse_error);
		if (parse_error.error != QJsonParseError::NoError || !doc.isObject()) {
			callback_error(tr("Invalid JSON response."));
			reply->deleteLater();
			return;
		}

		const QJsonObject data_obj = doc.object().value("results").toObject().value("data").toObject();

		const QJsonObject credentials_obj = data_obj.value("credentials").toObject();
		if (!credentials_obj.value("valid").toBool()) {
			const QString reason = credentials_obj.value("reason").toString();
			QString reason_msg;
			if (reason == QLatin1String("basic_password_expired"))
				reason_msg = tr("Password has expired.");
			else if (reason == QLatin1String("basic_event_id_mismatch"))
				reason_msg = tr("Event ID mismatch.");
			else if (reason == QLatin1String("basic_auth_required") || reason == QLatin1String("missing_authorization_header"))
				reason_msg = tr("Authentication required.");
			else
				reason_msg = tr("Invalid credentials.");
			callback(false, reason_msg);
			reply->deleteLater();
			return;
		}

		const QJsonObject event_obj = data_obj.value("event").toObject();
		if (event_obj.isEmpty()) {
			callback_error(tr("Event not found."));
			reply->deleteLater();
			return;
		}

		const QString event_name = event_obj.value("name").toString().trimmed();
		const QString organizer = event_obj.value("organizer").toString().trimmed();
		const QString date_str = event_obj.value("date").toString();
		const QString timezone = event_obj.value("timezone").toString().trimmed();

		const QDateTime event_date = QDateTime::fromString(date_str, Qt::ISODate);
		const QTimeZone event_tz = timezone.isEmpty() ? QTimeZone::utc() : QTimeZone(timezone.toUtf8());
		const QDateTime local_date = event_date.isValid() ? event_date.toTimeZone(event_tz) : QDateTime();

		const QString formatted_date = local_date.isValid()
			? QLocale().toString(local_date.date(), QLocale::ShortFormat)
			: date_str;
		const QString formatted_time = local_date.isValid()
			? QLocale().toString(local_date.time(), QLocale::ShortFormat)
			: QString();

		QStringList parts;
		if (!formatted_date.isEmpty())
			parts << formatted_date;
		if (!event_name.isEmpty())
			parts << event_name;
		if (!organizer.isEmpty())
			parts << organizer;
		if (!formatted_time.isEmpty())
			parts << formatted_time;

		callback(true, parts.join(QStringLiteral(", ")));
		reply->deleteLater();
	});
}

void OFeedClient::checkCredentials()
{
	if (status() != Status::Running)
		return;
	emit credentialCheckFired();

	const QString host_url = hostUrl();
	const QString event_id = eventId();
	const QString event_password = eventPassword();

	if (host_url.isEmpty() || event_id.isEmpty() || event_password.isEmpty())
		return;

	QUrl url(normalized_base_host_url(host_url));
	url.setPath(QStringLiteral("/rest/v1/events/%1/connection-check").arg(event_id));

	QNetworkRequest request(url);
	request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
	const QString combined = event_id + ":" + event_password;
	request.setRawHeader("Authorization", "Basic " + combined.toUtf8().toBase64());

	QNetworkReply *reply = m_networkManager->post(request, QByteArray("{}"));

	connect(reply, &QNetworkReply::finished, this, [this, reply]() {
		reply->deleteLater();

		const int http_status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
		if (reply->error() != QNetworkReply::NoError || http_status != 200)
			return;

		QJsonParseError parse_error;
		const QJsonDocument doc = QJsonDocument::fromJson(reply->readAll(), &parse_error);
		if (parse_error.error != QJsonParseError::NoError || !doc.isObject())
			return;

		const QJsonObject credentials_obj = doc.object().value("results").toObject().value("data").toObject().value("credentials").toObject();
		if (credentials_obj.isEmpty())
			return;

		const bool creds_valid = credentials_obj.value("valid").toBool();
		const bool was_valid = m_credentialsValid != 0;
		m_credentialsValid = creds_valid ? 1 : 0;
		emit credentialsStatusChanged(creds_valid);

		if (!creds_valid && was_valid && !m_credentialWarningShown) {
			m_credentialWarningShown = true;
			QMessageBox::warning(
				nullptr,
				tr("OFeed — Invalid Credentials"),
				tr("OFeed password is invalid or has expired.\n\nPlease open OFeed service settings and update your credentials.")
			);
		}
		if (creds_valid)
			m_credentialWarningShown = false;
	});
}

void OFeedClient::sendFile(QString name, QString request_path, QString file, std::function<void()> on_success, std::function<void()> on_done)
{
	// Create a multi-part request (like FormData in JS)
	auto *multi_part = new QHttpMultiPart(QHttpMultiPart::FormDataType);

	// Prepare the Authorization header with Bearer token
	QString combined = eventId() + ":" + eventPassword();
	QByteArray base_64_auth = combined.toUtf8().toBase64();
	QString auth_value = "Basic " + QString(base_64_auth);
	QByteArray auth_header = auth_value.toUtf8();

	// Add eventId field
	QHttpPart event_id_part;
	event_id_part.setHeader(QNetworkRequest::ContentDispositionHeader, QVariant(R"(form-data; name="eventId")"));
	event_id_part.setBody(eventId().toUtf8());
	multi_part->append(event_id_part);

	// Disable xml validation
	bool xmlValidation = runXmlValidation();
	if(!xmlValidation){
		QHttpPart validate_xml_part;
		validate_xml_part.setHeader(QNetworkRequest::ContentDispositionHeader, QVariant(R"(form-data; name="validateXml")"));
		validate_xml_part.setBody(QByteArray(xmlValidation ? "true" : "false"));
		multi_part->append(validate_xml_part);
		qfDebug() << "Upload without IOF XML validation, validateXml: " + QString(xmlValidation ? "true" : "false");
	}

	// Add xml content with fake filename that must be present
	QHttpPart file_part;
	file_part.setHeader(QNetworkRequest::ContentTypeHeader, QVariant("application/zlib"));
	file_part.setHeader(QNetworkRequest::ContentDispositionHeader, QVariant(R"(form-data; name="file"; filename="uploaded_file.xml")"));
	file_part.setBody(zlibCompress(file.toUtf8()));
	multi_part->append(file_part);

	// Create network request with authorization header
	QUrl request_url(hostUrl() + request_path);
	QNetworkRequest request(request_url);
	request.setRawHeader("Authorization", auth_header);

	// Send request
	QNetworkReply *reply = m_networkManager->post(request, multi_part);
	multi_part->setParent(reply);

	// Cleanup
	connect(reply, &QNetworkReply::finished, this, [reply, name, request_url, on_success, on_done]() {
		if(reply->error()) {
			auto err_msg = serviceName().toStdString() + " [" + name.toStdString() + "] " + request_url.toString().toStdString() + " : ";
			auto response_body = reply->readAll();
			if (!response_body.isEmpty())
				err_msg += response_body + " | ";
			qfError() << err_msg + reply->errorString().toStdString();
		}
		else {
			qfInfo() << serviceName().toStdString() + " [" + name.toStdString() + "]: ok";
			if (on_success)
				on_success();
		}
		reply->deleteLater();
		if (on_done)
			on_done();
	});
}

/// @brief Update competitors data by OFeed id or external id (QE id -> runs.id)
/// @param json_body body with the competitors data
/// @param competitor_or_external_id ofeed competitor id or external id (for QE runs.id)
/// @param using_external_id indicator which id is used - competitor (internal OFeed) or external (QE id from runs table)
/// @param on_done called with true when OFeed accepted the update
void OFeedClient::sendCompetitorUpdate(QString json_body, int competitor_or_external_id, bool using_external_id = true, std::function<void(bool)> on_done = nullptr)
{
	qfDebug() << serviceName().toStdString() + " [sendCompetitorUpdate] id:" << competitor_or_external_id
			 << "useExternalId:" << using_external_id
			 << "body:" << json_body;

	// Prepare the Authorization header base64 username:password
	QString combined = eventId() + ":" + eventPassword();
	QByteArray base_64_auth = combined.toUtf8().toBase64();
	QString auth_value = "Basic " + QString(base_64_auth);
	QByteArray auth_header = auth_value.toUtf8();

	// Create the URL for the PUT request
	QUrl url = hostUrl();
	if (using_external_id)
	{
		// qDebug() << serviceName().toStdString() + " - request to change competitor with QE id (run id): " << competitor_or_external_id;
		url.setPath(QStringLiteral("/rest/v1/events/%1/competitors/%2/external-id").arg(eventId(), QString::number(competitor_or_external_id)));
	}
	else
	{
		// qDebug() << serviceName().toStdString() + " - request to change competitor with OFeed id: " << competitor_or_external_id;
		url.setPath(QStringLiteral("/rest/v1/events/%1/competitors/%2").arg(eventId(), QString::number(competitor_or_external_id)));
	}

	// Create the network request
	QNetworkRequest request(url);
	request.setRawHeader("Authorization", auth_header);
	request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");

	// Send request
	QNetworkReply *reply = m_networkManager->put(request, json_body.toUtf8());

	connect(reply, &QNetworkReply::finished, this, [this, reply, using_external_id, competitor_or_external_id, on_done]()
			{
				bool ok = false;
				if(reply->error()) {
					qfError() << serviceName().toStdString() + " [competitor update]: " << reply->errorString()
							  << "HTTP status:" << reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt()
							  << "response:" << QString::fromUtf8(reply->readAll());
					// the competitor is not in OFeed (QE was restarted, the create request failed ...)
					if (using_external_id && reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt() == 404) {
						createCompetitorOrDefer(competitor_or_external_id);
					}
				}
				else {
					QByteArray response = reply->readAll();
					QJsonDocument json_response = QJsonDocument::fromJson(response);
					QJsonObject json_object = json_response.object();

					if (json_object.contains("error") && !json_object["error"].toBool()) {
						ok = true;
						QJsonObject results_object = json_object["results"].toObject();
						QJsonObject data_object = results_object["data"].toObject();

						if (data_object.contains("message")) {
							QString data_message = data_object["message"].toString();
							qfInfo() << serviceName().toStdString() + " [competitor details update]: " << data_message;
						} else {
							qfInfo() << serviceName().toStdString() + " [competitor details update]: ok, but no data message found.";
						}
					} else {
						qfError() << serviceName().toStdString() + " [competitor details update] Unexpected response: " << response;
					}
				}
				reply->deleteLater();
				if (on_done)
					on_done(ok); });
}

void OFeedClient::sendCompetitorAdded(QString json_body, int run_id)
{
	m_createsInFlight.insert(run_id);

	// Prepare the Authorization header base64 username:password
	QString combined = eventId() + ":" + eventPassword();
	QByteArray base_64_auth = combined.toUtf8().toBase64();
	QString auth_value = "Basic " + QString(base_64_auth);
	QByteArray auth_header = auth_value.toUtf8();

	// Create the URL for the POST request
	QUrl url(hostUrl() + "/rest/v1/events/" + eventId() + "/competitors");

	// Create the network request
	QNetworkRequest request(url);
	request.setRawHeader("Authorization", auth_header);
	request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");

	// Send request
	QNetworkReply *reply = m_networkManager->post(request, json_body.toUtf8());

	connect(reply, &QNetworkReply::finished, this, [this, reply, run_id]()
			{
				m_createsInFlight.remove(run_id);
				if(reply->error()) {
					qfError() << serviceName().toStdString() + " [new competitor]: " << reply->errorString()
							  << "HTTP status:" << reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt()
							  << "response:" << QString::fromUtf8(reply->readAll());
					// try again with the next change of the run
					m_pendingNewRuns.insert(run_id);
				}
				else {
					QByteArray response = reply->readAll();
					QJsonDocument json_response = QJsonDocument::fromJson(response);
					QJsonObject json_object = json_response.object();

					if (json_object.contains("error") && !json_object["error"].toBool()) {
						QJsonObject results_object = json_object["results"].toObject();
						QJsonObject data_object = results_object["data"].toObject();

						if (data_object.contains("message")) {
							QString data_message = data_object["message"].toString();
							qfInfo() << serviceName().toStdString() + " [new competitor]: " << data_message;
						} else {
							qfInfo() << serviceName().toStdString() + " [new competitor]: ok, but no data message found.";
						}
					} else {
						qfError() << serviceName().toStdString() + " [new competitor] Unexpected response: " << response;
					}
				}
				reply->deleteLater(); });
}

void OFeedClient::sendCompetitorDeleted(int run_id)
{
	// Prepare the Authorization header base64 username:password
	QString combined = eventId() + ":" + eventPassword();
	QByteArray base_64_auth = combined.toUtf8().toBase64();
	QString auth_value = "Basic " + QString(base_64_auth);
	QByteArray auth_header = auth_value.toUtf8();

	// Create the URL for the POST request
	QUrl url(hostUrl() + "/rest/v1/events/" + eventId() + "/competitors/" + QString::number(run_id) + "/external-id");

	// Create the network request
	QNetworkRequest request(url);
	request.setRawHeader("Authorization", auth_header);
	request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");

	// Send request
	QNetworkReply *reply = m_networkManager->deleteResource(request);

	connect(reply, &QNetworkReply::finished, this, [=]()
			{
				if(reply->error()) {
					qfError() << serviceName().toStdString() + " [deleted competitor]: " << reply->errorString()
							  << "HTTP status:" << reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt()
							  << "response:" << QString::fromUtf8(reply->readAll());
				}
				else {
					QByteArray response = reply->readAll();
					QJsonDocument json_response = QJsonDocument::fromJson(response);
					QJsonObject json_object = json_response.object();

					if (json_object.contains("error") && !json_object["error"].toBool()) {
						QJsonObject results_object = json_object["results"].toObject();

						if (results_object.contains("data")) {
							QString data = results_object["data"].toString();
							qfInfo() << serviceName().toStdString() + " [deleted competitor (external id)]: " << data;
						} else {
							qfInfo() << serviceName().toStdString() + " [deleted competitor (external id)]: ok, but no data message found.";
						}
					} else {
						qfError() << serviceName().toStdString() + " [deleted competitor (external id)] Unexpected response: " << response;
					}
				}
				reply->deleteLater(); });
}

namespace {
QString jsonToString(const QJsonObject &o) {
	QJsonDocument doc(o);
	return QString::fromUtf8(doc.toJson(QJsonDocument::Compact));
}
}

void OFeedClient::sendGraphQLRequest(const QString &query,
									 const QJsonObject &variables,
									 std::function<void(QJsonObject)> callback,
									 bool withAuthorization = false) {
	QUrl url(hostUrl() + "/graphql");
	QNetworkRequest request(url);
	request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");

	// Add authorization header if required
	if (withAuthorization) {
		QString combined = eventId() + ":" + eventPassword();
		QByteArray auth = "Basic " + combined.toUtf8().toBase64();
		request.setRawHeader("Authorization", auth);
	}

	// Construct the JSON payload for the GraphQL request
	QJsonObject payload;
	payload["query"] = query;
	if (!variables.isEmpty()) {
		payload["variables"] = variables;
	}

	// Compact JSON
	QByteArray request_body =
			QJsonDocument(payload).toJson(QJsonDocument::Compact);

	// Send the POST request
	QNetworkReply *reply = m_networkManager->post(request, request_body);

	connect(reply, &QNetworkReply::finished, this, [=]() {
		// Default value
		QJsonObject data_object;

		if (reply->error()) {
			qfError() << serviceName().toStdString() + " [GraphQL request]: "
					  << reply->errorString();
		} else {
			QByteArray response = reply->readAll();
			QJsonDocument json_response = QJsonDocument::fromJson(response);
			QJsonObject json_object = json_response.object();

			if (json_object.contains("errors")) {
				qfError() << serviceName().toStdString() +
							 " [GraphQL request] Errors in response: "
						  << jsonToString(json_object);
			} else if (json_object.contains("data")) {
				data_object = json_object["data"].toObject();
			} else {
				qfError() << serviceName().toStdString() +
							 " [GraphQL request] Unexpected response: "
						  << jsonToString(json_object);
			}
		}
		reply->deleteLater();

		// Call the callback with the resulting data_object
		callback(data_object);
	});
}

void OFeedClient::getChangesByOrigin(const QString &origin, std::function<void()> on_done)
{
	if (m_changesProcessingInProgress) {
		qfDebug() << serviceName() << "changes processing already in progress, skipping";
		if (on_done)
			on_done();
		return;
	}
	m_changesProcessingInProgress = true;

	try
	{
		QDateTime last_changelog_call_value = lastChangelogCall(origin);
		QDateTime initial_value = QDateTime::fromSecsSinceEpoch(0); // Unix epoch

		// competitor details are needed to create a new runner (competitor_create), so no extra request per runner
		const QString graphQLquery = R"(
		query ChangelogByEvent($eventId: String!, $origin: String, $since: DateTime) {
			changelogByEvent(eventId: $eventId, origin: $origin, since: $since) {
				id
				processed
				type
				previousValue
				newValue
				origin
				competitor {
					id
					externalId
					firstname
					lastname
					registration
					card
					note
					startTime
					class {
						externalId
					}
				}
				createdAt
			}
		}
		)";

		QJsonObject variables;
		variables["eventId"] = eventId();
		variables["origin"] = origin;

		// no "since" for the very first call
		if (last_changelog_call_value != initial_value)
			variables["since"] = last_changelog_call_value.toString(Qt::ISODate);

		sendGraphQLRequest(graphQLquery, variables, [this, origin, on_done](QJsonObject data)
						   {
			if (!data.isEmpty())
			{
				// Check if the "data" key exists and is an array
				if (data.contains("changelogByEvent") && data["changelogByEvent"].isArray()) {
					QJsonArray changelog_array = data["changelogByEvent"].toArray();

					if (changelog_array.isEmpty()) {
						qfInfo() << "No changes from origin: " << origin;
					}
					else {
						// Process the data
						const QDateTime next_since = processCompetitorsChanges(changelog_array);

						// "since" is inclusive and comes from the server time, so no clock skew and no gap
						if (next_since.isValid())
							setLastChangelogCall(origin, next_since);
					}
				}
			}
			m_changesProcessingInProgress = false;
			if (on_done)
				on_done();
		}, true);
	}
	catch (const std::exception &e)
	{
		qCritical() << tr("Exception occurred while getting changes by origin: ") << e.what();
		m_changesProcessingInProgress = false;
		if (on_done)
			on_done();
	}
}

/// @return value for the next "since": createdAt of the first change that failed (to retry it),
/// otherwise createdAt of the latest change; invalid when there is nothing to move to
QDateTime OFeedClient::processCompetitorsChanges(QJsonArray data_array)
{
	if (data_array.isEmpty())
	{
		return {};
	}
	QDateTime first_failed;
	QDateTime latest;

	// Prevent back-sending of changes that originate from OFeed. The flag must
	// stay true until all DB events queued by the SQL updates below are processed,
	// so it is cleared via singleShot(0) which fires after those queued events.
	m_processingOFeedChanges = true;

	for (const QJsonValue &value : data_array)
	{

		if (!value.isObject())
		{
			continue;
		}

		QJsonObject change = value.toObject();
		const QDateTime change_created_at = QDateTime::fromString(change["createdAt"].toString(), Qt::ISODate);
		if (change_created_at.isValid() && (!latest.isValid() || change_created_at > latest))
			latest = change_created_at;
		if (change["processed"].toBool())
			continue;
		const int protocol_id = change["id"].toInt();
		if (m_changeFailures.value(protocol_id) >= k_max_change_attempts)
			continue; // given up already, it only stays in the changelog

		// Extract values
		QString type = change["type"].toString();
		QString previous_value = change["previousValue"].toString();
		QString new_value = change["newValue"].toString();
		QString origin = change["origin"].toString();
		QDateTime created_at = QDateTime::fromString(change["createdAt"].toString(), Qt::ISODate);

		// Retrieve competitor and details
		auto competitor = change.value("competitor").toObject();
		QString external_id_str = competitor["externalId"].toString();
		int runs_id = external_id_str.toInt();
		qfDebug() << "Processing change for competitorId (OFeed externalId):" << runs_id << ", type:" << type << ", " << previous_value << " -> " << new_value;

		bool is_dns = type == QLatin1String("status_change") && (new_value == QLatin1String("DidNotStart") || new_value == QLatin1String("Cancelled"));
		if (origin == k_start_changelog_origin && created_at.isValid() && !is_dns)
			processCorridorTimeUpdate(runs_id, created_at);

		// Handle each type of change
		if (type == "si_card_change")
		{
			processCardChange(runs_id, new_value, origin == k_office_changelog_origin);
		}
		else if (type == "status_change")
		{
			processStatusChange(runs_id, new_value);
		}
		else if (type == "note_change")
		{
			processNoteChange(runs_id, new_value);
		}
		else if (type == "start_time_change" || type == "firstname_change" || type == "lastname_change" || type == "class_change")
		{
			if (!processCompetitorChange(type, runs_id, competitor)) {
				// retried by the next "since"
				if (retryChange(protocol_id) && change_created_at.isValid() && !first_failed.isValid())
					first_failed = change_created_at;
				continue;
			}
			m_changeFailures.remove(protocol_id);
		}
		else if (type == "competitor_create")
		{
			// marked as processed in the async callback, only after the runner is stored
			if (!processNewRunner(change) && retryChange(protocol_id) && change_created_at.isValid() && !first_failed.isValid())
				first_failed = change_created_at;
			storeChange(change);
			continue;
		}
		else
		{
			qfInfo() << "Unsupported change type: " << type.toStdString();
			continue;
		}

		// Store the processed change
		storeChange(change);
		markChangelogEntryAsProcessed(change["id"].toInt());
	}

	QTimer::singleShot(0, this, [this]() {
		m_processingOFeedChanges = false;
	});
	return first_failed.isValid() ? first_failed : latest;
}

void OFeedClient::markChangelogEntryAsProcessed(int protocolId)
{
	QString mutation = R"(
	mutation MarkChangelogProcessed($eventId: String!, $protocolId: Int!, $processedByType: ProtocolProcessedByType, $processedBySource: String!) {
		markChangelogProcessed(eventId: $eventId, protocolId: $protocolId, processedByType: $processedByType, processedBySource: $processedBySource) {
			id
			processed
		}
	}
	)";

	QJsonObject variables;
	variables["eventId"] = eventId();
	variables["protocolId"] = protocolId;
	variables["processedByType"] = QString("SYSTEM");
	variables["processedBySource"] = QString("IT");

	sendGraphQLRequest(mutation, variables, [protocolId](QJsonObject data) {
		if (data.isEmpty()) {
			qfWarning() << serviceName().toStdString() + " Failed to mark changelog entry " << protocolId << " as processed";
		} else {
			qfDebug() << serviceName().toStdString() + " Changelog entry " << protocolId << " marked as processed";
		}
	}, true);
}

void OFeedClient::processCardChange(int runs_id, const QString &new_value, bool office)
{
	if (office) {
		// card changed by the office is a change of the competitor card, not just of the run at the start
		const int competitor_id = getPlugin<RunsPlugin>()->competitorForRun(runs_id);
		if (competitor_id > 0) {
			qf::gui::framework::Application::instance()->qxSql()->updateRecord("competitors", competitor_id, QVariantMap{{"siId", new_value.toInt()}}, this);
			try {
				qf::core::sql::Query q;
				q.prepare("UPDATE runs SET siId=:siId WHERE competitorId=:competitorId", qf::core::Exception::Throw);
				q.bindValue(":competitorId", competitor_id);
				q.bindValue(":siId", new_value.toInt());
				q.exec(qf::core::Exception::Throw);
			}
			catch (const std::exception &e) {
				qfError() << tr("Exception occurred while executing query: ") << e.what();
			}
			return;
		}
	}
	qf::core::sql::Query q;
	try
	{
		q.prepare("UPDATE runs SET siId=:siId WHERE id=:runsId", qf::core::Exception::Throw);
		q.bindValue(":runsId", runs_id);
		q.bindValue(":siId", new_value.toInt());
		if (!q.exec(qf::core::Exception::Throw))
		{
			qfError() << tr("Database query failed: ") << q.lastError().text();
		}
	}
	catch (const std::exception &e)
	{
		qCritical() << tr("Exception occurred while executing query: ") << e.what();
	}
	catch (...)
	{
		qCritical() << tr("Unknown exception occurred while executing query.");
	}
}

void OFeedClient::processStatusChange(int runs_id, const QString &new_value)
{
	// entry cancellation comes as status Cancelled, the runner stays in QE but does not start
	bool notStart = new_value == "DidNotStart" || new_value == "Cancelled";

	qf::core::sql::Query q;
	try
	{
		q.prepare("UPDATE runs SET notStart=:notStart WHERE id=:runsId", qf::core::Exception::Throw);
		q.bindValue(":runsId", runs_id);
		q.bindValue(":notStart", notStart);
		if (!q.exec(qf::core::Exception::Throw))
		{
			qfError() << tr("Database query failed: ") << q.lastError().text();
		}
	}
	catch (const std::exception &e)
	{
		qCritical() << tr("Exception occurred while executing query: ") << e.what();
	}
	catch (...)
	{
		qCritical() << tr("Unknown exception occurred while executing query.");
	}
}

void OFeedClient::processNoteChange(int runs_id, const QString &new_value)
{
	int competitor_id = getPlugin<RunsPlugin>()->competitorForRun(runs_id);

	qf::core::sql::Query q;
	try
	{
		q.prepare("UPDATE competitors SET note = CASE WHEN note IS NULL OR note = '' THEN :newNote ELSE note || ', ' || :newNote END WHERE id = :competitorId", qf::core::Exception::Throw);
		q.bindValue(":competitorId", competitor_id);
		q.bindValue(":newNote", new_value);
		if (!q.exec(qf::core::Exception::Throw))
		{
			qfError() << tr("Database query failed: ") << q.lastError().text();
		}
	}
	catch (const std::exception &e)
	{
		qCritical() << tr("Exception occurred while executing query: ") << e.what();
	}
	catch (...)
	{
		qCritical() << tr("Unknown exception occurred while executing query.");
	}
}

void OFeedClient::processCorridorTimeUpdate(int runs_id, const QDateTime &created_at)
{
	qf::core::sql::Query q;
	try
	{
		q.prepare("UPDATE runs SET corridorTime = :createdAt "
				  "WHERE id = :runsId "
				  "AND (corridorTime IS NULL OR corridorTime > :createdAt2)",
				  qf::core::Exception::Throw);
		q.bindValue(":runsId", runs_id);
		q.bindValue(":createdAt", created_at);
		q.bindValue(":createdAt2", created_at);
		if (!q.exec(qf::core::Exception::Throw))
			qfError() << tr("Database query failed: ") << q.lastError().text();
	}
	catch (const std::exception &e)
	{
		qCritical() << tr("Exception occurred while executing query: ") << e.what();
	}
	catch (...)
	{
		qCritical() << tr("Unknown exception occurred while executing query.");
	}
}

/// @param start_time ISO date time from OFeed, empty clears the start time
/// @return false when the start time is not valid and nothing was changed
bool OFeedClient::setRunStartTime(int run_id, const QString &start_time)
{
	QVariant start_time_ms; // NULL
	if (!start_time.isEmpty()) {
		const QDateTime dt = QDateTime::fromString(start_time, Qt::ISODate);
		if (!dt.isValid()) {
			qfWarning() << tr("Start time of the run %1 is not valid, not set: %2").arg(run_id).arg(start_time);
			return false;
		}
		const int stage_id = getPlugin<EventPlugin>()->stageIdForRun(run_id);
		start_time_ms = static_cast<int>(getPlugin<EventPlugin>()->stageStartDateTime(stage_id).msecsTo(dt));
	}
	// source == this, so the change is not sent back to OFeed
	qf::gui::framework::Application::instance()->qxSql()->updateRecord("runs", run_id, QVariantMap{{"startTimeMs", start_time_ms}}, this);
	return true;
}

/// Changes of the competitor data made at OFeed. The current values of the competitor are used (not the
/// changelog newValue, which is a text of the value, class change has the OFeed class id there).
/// @return false when the change was not applied (the changelog entry is left unprocessed and retried)
bool OFeedClient::processCompetitorChange(const QString &type, int runs_id, const QJsonObject &competitor)
{
	auto *event_plugin = getPlugin<EventPlugin>();
	if (event_plugin->stageIdForRun(runs_id) != event_plugin->currentStageId()) {
		qfWarning() << tr("Change %1 skipped, run %2 is not in the current stage").arg(type).arg(runs_id);
		return false;
	}
	if (type == QLatin1String("start_time_change"))
		return setRunStartTime(runs_id, competitor.value("startTime").toString());

	const int competitor_id = getPlugin<RunsPlugin>()->competitorForRun(runs_id);
	if (competitor_id <= 0) {
		qfWarning() << tr("Change %1 skipped, competitor of the run %2 not found").arg(type).arg(runs_id);
		return false;
	}
	QVariantMap record;
	if (type == QLatin1String("firstname_change")) {
		record["firstName"] = competitor.value("firstname").toString();
	}
	else if (type == QLatin1String("lastname_change")) {
		record["lastName"] = competitor.value("lastname").toString();
	}
	else if (type == QLatin1String("class_change")) {
		const int class_id = competitor.value("class").toObject().value("externalId").toString().toInt();
		try {
			qf::core::sql::Query q;
			q.prepare("SELECT id FROM classes WHERE id=:classId", qf::core::Exception::Throw);
			q.bindValue(":classId", class_id);
			if (!q.exec(qf::core::Exception::Throw) || !q.next()) {
				qfWarning() << tr("Class change skipped, class %1 does not exist in QE").arg(class_id);
				return false;
			}
		}
		catch (const std::exception &e) {
			qfError() << tr("Exception occurred while executing query: ") << e.what();
			return false;
		}
		record["classId"] = class_id;
	}
	// source == this, so the change is not sent back to OFeed
	return qf::gui::framework::Application::instance()->qxSql()->updateRecord("competitors", competitor_id, record, this);
}

/// counts a failure of the changelog entry
/// @return true when the entry should be retried by the next "since", false when the limit is reached
bool OFeedClient::retryChange(int protocol_id)
{
	const int failures = ++m_changeFailures[protocol_id];
	if (failures >= k_max_change_attempts) {
		qfError() << tr("Changelog entry %1 failed %2 times, giving up, it stays unprocessed in OFeed").arg(protocol_id).arg(failures);
		return false;
	}
	return true;
}

/// @param change competitor_create changelog entry with the competitor details
/// @return false when the runner could not be stored (the entry is then retried by the next "since")
bool OFeedClient::processNewRunner(const QJsonObject &change)
{
	const QJsonObject competitor = change.value("competitor").toObject();
	const int protocol_id = change.value("id").toInt();
	const int ofeed_competitor_id = competitor.value("id").toInt();
	const QString registration = competitor.value("registration").toString();
	const int current_stage = getPlugin<EventPlugin>()->currentStageId();
	qfDebug() << "Storing a new runner (OFeed id):" << ofeed_competitor_id;

	// the runner may be stored already, when the externalId could not be sent to OFeed the last time
	int run_id = 0;
	if (!registration.isEmpty()) {
		try {
			qf::core::sql::Query q;
			q.prepare("SELECT runs.id FROM runs JOIN competitors ON competitors.id = runs.competitorId "
					  "WHERE competitors.registration = :registration AND runs.stageId = :stageId",
					  qf::core::Exception::Throw);
			q.bindValue(":registration", registration);
			q.bindValue(":stageId", current_stage);
			if (q.exec(qf::core::Exception::Throw) && q.next())
				run_id = q.value(0).toInt();
		}
		catch (const std::exception &e) {
			// creating the runner without knowing whether it exists could duplicate it
			qfError() << tr("Lookup of the existing runner failed: ") << e.what();
			return false;
		}
	}

	if (run_id > 0) {
		qfInfo() << tr("Runner (OFeed id) %1 exists in QE already (run id %2), only the externalId is sent").arg(ofeed_competitor_id).arg(run_id);
	}
	else {
		// Create the competitor in QE
		Competitors::CompetitorDocument doc;
		doc.loadForInsert();
		doc.setValue("firstName", competitor.value("firstname").toString());
		doc.setValue("lastName", competitor.value("lastname").toString());
		doc.setValue("registration", registration);
		doc.setValue("classid", competitor.value("class").toObject().value("externalId").toString());
		doc.setSiid(competitor.value("card").toInt());
		doc.setValue("note", competitor.value("note").toString());

		// Save emits db event
		try {
			doc.save();
		}
		catch (const std::exception &e) {
			// e.g. class externalId from OFeed does not exist locally; leave the changelog entry unprocessed
			qfError() << tr("Failed to store new runner (OFeed id): ") << ofeed_competitor_id << e.what();
			return false;
		}

		// Get runs.id for current stage
		run_id = doc.runsIds().value(current_stage - 1);
		// the insert DB event is delivered later, mark the run so it is not sent back to OFeed
		m_runsInsertedFromOFeed.insert(run_id);

		// start time matters in classes with a start list; source == this, so it is not sent back
		setRunStartTime(run_id, competitor.value("startTime").toString());
	}

	// Update externalId at OFeed
	const QString json_body = QStringLiteral(R"({"origin":"IT","externalId":"%1"})").arg(run_id);
	const QString origin = change.value("origin").toString();
	const QDateTime created_at = QDateTime::fromString(change.value("createdAt").toString(), Qt::ISODate);
	// mark as processed only when OFeed has the externalId, otherwise later updates from QE would create a duplicate
	sendCompetitorUpdate(json_body, ofeed_competitor_id, false, [this, protocol_id, origin, created_at](bool ok) {
		if (ok) {
			m_changeFailures.remove(protocol_id);
			markChangelogEntryAsProcessed(protocol_id);
			return;
		}
		// "since" was moved already, go back so that the entry is loaded again and only this PUT is repeated
		qfError() << tr("Failed to set externalId at OFeed, changelog entry %1").arg(protocol_id);
		if (retryChange(protocol_id) && created_at.isValid() && lastChangelogCall(origin) > created_at)
			setLastChangelogCall(origin, created_at);
	});
	return true;
}

void OFeedClient::storeChange(const QJsonObject &change)
{
	int current_stage = getPlugin<EventPlugin>()->currentStageId();
	auto competitor = change.value("competitor").toObject();

	QString external_id_str = competitor["externalId"].toString();
	int runs_id = external_id_str.toInt();
	int competitor_id = getPlugin<RunsPlugin>()->competitorForRun(runs_id);
	QString no_data = "(undefined)";

	QString previous_value = change["previousValue"].isString() ? change["previousValue"].toString() : no_data;
	QString new_value = change["newValue"].isString() ? change["newValue"].toString() : QString();
	QString firstname = competitor["firstname"].isString() ? competitor["firstname"].toString() : no_data;
	QString lastname = competitor["lastname"].isString() ? competitor["lastname"].toString() : no_data;

	// QJsonDocument change_json_doc(change);
	// QString change_json = QString::fromUtf8(change_json_doc.toJson(QJsonDocument::Compact));

	int change_id = change["id"].toInt();
	auto created = QDateTime::fromString(change["createdAt"].toString(), Qt::ISODate);

	auto *app = qf::gui::framework::Application::instance();
	QVariantMap rec {
		{"data_type", change.value("type").toString()},
		{"data", new_value},
		{"foreign_table", "runs"},
		{"foreign_id", runs_id},
		{"orig_data", previous_value},
		{"source", "OFeed"},
		{"user_id", competitor_id},
		{"stage_id", current_stage},
		{"change_id", change_id},
		{"created", created},
		{"status", "Accepted"},
		{"status_message", firstname + " " + lastname + ": " + previous_value + " -> " + new_value},
	};
	try
	{
		// createRecord emits the record change itself
		app->qxSql()->createRecord("qxchanges", rec, this);
	}
	catch (const std::exception &e)
	{
		qCritical() << "Exception occurred while storing the change:" << e.what();
	}
	catch (...)
	{
		qCritical() << "Unknown exception occurred while storing the change.";
	}
}

namespace {
QString getIofResultStatus(
	int time,
	bool is_disq,
	bool is_disq_by_organizer,
	bool is_miss_punch,
	bool is_bad_check,
	bool is_did_not_start,
	bool is_did_not_finish,
	bool is_not_competing)
{
	// IOF xml 3.0 statuses:
	// OK (finished and validated)
	// Finished (finished but not yet validated.)
	// MissingPunch
	// Disqualified (for some other reason than a missing punch)
	// DidNotFinish
	// Active
	// Inactive
	// OverTime
	// SportingWithdrawal
	// NotCompeting
	// DidNotStart
	// Status flags take priority over time checks
	if (is_not_competing)
		return "NotCompeting";
	if (is_did_not_start)
		return "DidNotStart";
	if (is_miss_punch)
		return "MissingPunch";
	if (is_did_not_finish)
		return "DidNotFinish";
	if (is_bad_check || is_disq_by_organizer || is_disq)
		return "Disqualified";
	if (time > 0)
		return "OK";
	return "Inactive";
}

QString datetime_to_string(const QDateTime &dt)
{
	return quickevent::core::Utils::dateTimeToIsoStringWithUtcOffset(dt);
}
}

void OFeedClient::onCompetitorAdded(int competitor_id)
{
	if (competitor_id == 0)
	{
		return;
	}

	int INT_INITIAL_VALUE = -1;

	int stage_id = getPlugin<EventPlugin>()->currentStageId();
	QDateTime stage_start_date_time = getPlugin<EventPlugin>()->stageStartDateTime(stage_id);
	qf::core::sql::Query q;
	q.exec("SELECT competitors.registration, "
		   "competitors.startNumber, "
		   "competitors.firstName, "
		   "competitors.lastName, "
		   "competitors.note, "
		   "clubs.name AS organisationName, "
		   "clubs.abbr AS organisationAbbr, "
		   "classes.id AS classId, "
		   "runs.id AS runId, "
		   "runs.siId, "
		   "runs.disqualified, "
		   "runs.disqualifiedByOrganizer, "
		   "runs.misPunch, "
		   "runs.badCheck, "
		   "runs.notStart, "
		   "runs.notFinish, "
		   "runs.notCompeting, "
		   "runs.startTimeMs, "
		   "runs.finishTimeMs, "
		   "runs.timeMs "
		   "FROM runs "
		   "INNER JOIN competitors ON competitors.id = runs.competitorId "
		   "LEFT JOIN relays ON relays.id = runs.relayId  "
		   "LEFT JOIN clubs ON substr(competitors.registration, 1, 3) = clubs.abbr "
		   "INNER JOIN classes ON classes.id = competitors.classId OR classes.id = relays.classId  "
		   "WHERE competitors.id=" QF_IARG(competitor_id) " AND runs.stageId=" QF_IARG(stage_id),
		   qf::core::Exception::Throw);
	if (q.next())
	{
		int run_id = q.value("runId").toInt();
		QString registration = q.value(QStringLiteral("registration")).toString();
		QString first_name = q.value(QStringLiteral("firstName")).toString();
		QString last_name = q.value(QStringLiteral("lastName")).toString();
		int card_number = q.value(QStringLiteral("siId")).toInt();
		QString organisation_name = q.value(QStringLiteral("organisationName")).toString();
		QString organisation_abbr = q.value(QStringLiteral("organisationAbbr")).toString();
		QString organisation = !organisation_abbr.isEmpty() ? organisation_name : registration.left(3);
		int class_id = q.value(QStringLiteral("classId")).toInt();
		QString nationality = "";
		QString origin = "IT";
		QString note = q.value(QStringLiteral("note")).toString();;

		// Start bib
		int start_bib = INT_INITIAL_VALUE;
		QVariant start_bib_variant = q.value(QStringLiteral("startNumber"));
		if (!start_bib_variant.isNull())
		{
			start_bib = start_bib_variant.toInt();
		}

		// Start time
		int start_time = INT_INITIAL_VALUE;
		QVariant start_time_variant = q.value(QStringLiteral("startTimeMs"));
		if (!start_time_variant.isNull())
		{
			start_time = start_time_variant.toInt();
		}

		// Finish time
		int finish_time = INT_INITIAL_VALUE;
		QVariant finish_time_variant = q.value(QStringLiteral("finishTimeMs"));
		if (!finish_time_variant.isNull())
		{
			finish_time = finish_time_variant.toInt();
		}

		// Time
		int running_time = INT_INITIAL_VALUE;
		QVariant running_time_variant = q.value(QStringLiteral("timeMs"));
		if (!running_time_variant.isNull())
		{
			running_time = running_time_variant.toInt();
		}

		// Status
		bool is_disq = q.value(QStringLiteral("disqualified")).toBool();
		bool is_disq_by_organizer = q.value(QStringLiteral("disqualifiedByOrganizer")).toBool();
		bool is_miss_punch = q.value(QStringLiteral("misPunch")).toBool();
		bool is_bad_check = q.value(QStringLiteral("badCheck")).toBool();
		bool is_did_not_start = q.value(QStringLiteral("notStart")).toBool();
		bool is_did_not_finish = q.value(QStringLiteral("notFinish")).toBool();
		bool is_not_competing = q.value(QStringLiteral("notCompeting")).toBool();
		QString status = getIofResultStatus(running_time, is_disq, is_disq_by_organizer, is_miss_punch, is_bad_check, is_did_not_start, is_did_not_finish, is_not_competing);

		// Use std::stringstream to build the JSON string
		std::stringstream json_payload;

		// Setup common values
		json_payload << "{"
					 << R"("origin":")" << origin.toStdString() << R"(",)"
					 << R"("firstname":")" << first_name.toStdString() << R"(",)"
					 << R"("lastname":")" << last_name.toStdString() << R"(",)"
					 << R"("registration":")" << registration.toStdString() << R"(",)"
					 << R"("organisation":")" << organisation.toStdString() << R"(",)"
					 << R"("status":")" << status.toStdString() << R"(",)"
					 << R"("note":")" << note.toStdString() << R"(",)";

		if (nationality != "")
		{
			json_payload << R"("nationality":")" << nationality.toStdString() << R"(",)";
		}

		// External ids
		json_payload << R"("classExternalId":")" << class_id << R"(",)"
						 << R"("externalId":")" << run_id << R"(",)";

		// Card number - QE saves 0 for empty si card
		if (card_number != 0)
		{
			json_payload << R"("card":)" << card_number << ",";
		}

		// Finish time
		if (finish_time != INT_INITIAL_VALUE)
		{
			json_payload << R"("finishTime":")" << datetime_to_string(stage_start_date_time.addMSecs(finish_time)).toStdString() << R"(",)";
		}

		// Star time
		if (start_time != INT_INITIAL_VALUE)
		{
			json_payload << R"("startTime":")" << datetime_to_string(stage_start_date_time.addMSecs(start_time)).toStdString() << R"(",)";
		}

		// Start bib
		if (start_bib != INT_INITIAL_VALUE)
		{
			json_payload << R"("bibNumber":)" << start_bib << ",";
		}

		//  Competitor's time
		if (running_time != INT_INITIAL_VALUE)
		{
			json_payload << R"("time":)" << running_time / 1000 << ",";
		}

		// Get the final JSON string
		std::string json_str = json_payload.str();

		// Remove the trailing comma if necessary
		if (json_str.back() == ',')
		{
			json_str.pop_back();
		}

		json_str += "}";

		// Convert std::string to QString
		QString json_qstr = QString::fromStdString(json_str);

		qfInfo() << serviceName().toStdString() + " [onCompetitorAdded] sending body:" << json_qstr;
		sendCompetitorAdded(json_qstr, run_id);
	}
}

bool OFeedClient::canCreateCompetitor(int run_id) const
{
	// OFeed requires the start time in classes with drawn start times
	qf::core::sql::Query q;
	q.exec("SELECT runs.startTimeMs, COALESCE(classdefs.startIntervalMin, 0) AS startIntervalMin "
		   "FROM runs "
		   "INNER JOIN competitors ON competitors.id = runs.competitorId "
		   "LEFT JOIN relays ON relays.id = runs.relayId "
		   "LEFT JOIN classdefs ON (classdefs.classId = competitors.classId OR classdefs.classId = relays.classId) "
		   "AND classdefs.stageId = runs.stageId "
		   "WHERE runs.id=" QF_IARG(run_id), qf::core::Exception::Throw);
	return q.next() && (!q.value("startTimeMs").isNull() || q.value("startIntervalMin").toInt() == 0);
}

/// @brief Creates the competitor in OFeed, when the start time is not set yet, waits for it
void OFeedClient::createCompetitorOrDefer(int run_id)
{
	if (m_createsInFlight.contains(run_id)) {
		return;
	}
	if (!canCreateCompetitor(run_id)) {
		qfInfo() << serviceName().toStdString() + " [new competitor]: not ready to be created in OFeed (start time or competitor missing), run id:" << run_id;
		m_pendingNewRuns.insert(run_id);
		return;
	}
	m_pendingNewRuns.remove(run_id);
	onCompetitorAdded(getPlugin<RunsPlugin>()->competitorForRun(run_id));
}

void OFeedClient::onRunChanged(int run_id, const QVariantMap &run_fields, const QVariantMap &competitor_fields)
{
	int stage_id = getPlugin<EventPlugin>()->currentStageId();
	QDateTime stage_start_date_time = getPlugin<EventPlugin>()->stageStartDateTime(stage_id);

	// field names are normalized in queueRunChange()
	auto field_value = [&run_fields](const QString &field_lower) -> QVariant {
		return run_fields.value(field_lower);
	};
	auto has_field = [&field_value](const QString &field_lower) -> bool {
		return field_value(field_lower).isValid();
	};

	std::stringstream json_payload;
	json_payload << "{"
				 << R"("origin":"IT",)"
				 << R"("useExternalId":true,)";

	bool has_fields = false;

	// Card (siId)
	if (has_field("siid")) {
		int card = field_value("siid").toInt();
		if (card != 0) {
			json_payload << R"("card":)" << card << ",";
			has_fields = true;
		}
	}

	// Start time
	// WARNING: OFeed takes startTime as a request for a start slot and answers 409 when the slot
	// is not free, which would drop the whole request including the result. A card read-out of a
	// class with drawn start times brings the punched start time along with the finish time and
	// OFeed knows such start time already, so it is not sent with the result. Classes without
	// drawn start times are handled after the result is sent, see below.
	bool is_result = run_fields.contains(QStringLiteral("finishtimems")) || run_fields.contains(QStringLiteral("timems"));
	int start_time_ms = has_field("starttimems")? field_value("starttimems").toInt(): 0;
	if (start_time_ms > 0 && !is_result) {
		json_payload << R"("startTime":")" << datetime_to_string(stage_start_date_time.addMSecs(start_time_ms)).toStdString() << R"(",)";
		has_fields = true;
	}

	// Finish time
	if (has_field("finishtimems")) {
		int ms = field_value("finishtimems").toInt();
		if (ms > 0) {
			json_payload << R"("finishTime":")" << datetime_to_string(stage_start_date_time.addMSecs(ms)).toStdString() << R"(",)";
			has_fields = true;
		}
	}

	// Running time
	if (has_field("timems")) {
		int ms = field_value("timems").toInt();
		if (ms > 0) {
			json_payload << R"("time":)" << ms / 1000 << ",";
			has_fields = true;
		}
	}

	// Leg (relay stage number)
	if (has_field("leg")) {
		QVariant v = field_value("leg");
		if (!v.isNull()) {
			json_payload << R"("leg":)" << v.toInt() << ",";
			has_fields = true;
		}
	}

	// Relay team
	if (has_field("relayid")) {
		QVariant v = field_value("relayid");
		if (!v.isNull()) {
			json_payload << R"("teamId":)" << v.toInt() << ",";
			has_fields = true;
		}
	}

	// Status — any flag or time change requires a DB read to compute the final value
	static const QSet<QString> status_source_fields = {
		"disqualified", "disqualifiedbyorganizer", "mispunch", "badcheck",
		"notstart", "notfinish", "notcompeting", "finishtimems", "timems"
	};
	bool has_status_change = false;
	for (const auto &[key, _] : run_fields.asKeyValueRange()) {
		if (status_source_fields.contains(key)) {
			has_status_change = true;
			break;
		}
	}
	if (has_status_change) {
		qf::core::sql::Query q;
		q.exec("SELECT disqualified, disqualifiedByOrganizer, misPunch, badCheck, "
			   "notStart, notFinish, notCompeting, timeMs "
			   "FROM runs WHERE id=" QF_IARG(run_id), qf::core::Exception::Throw);
		if (q.next()) {
			QString status = getIofResultStatus(
				q.value("timeMs").toInt(),
				q.value("disqualified").toBool(),
				q.value("disqualifiedByOrganizer").toBool(),
				q.value("misPunch").toBool(),
				q.value("badCheck").toBool(),
				q.value("notStart").toBool(),
				q.value("notFinish").toBool(),
				q.value("notCompeting").toBool());
			json_payload << R"("status":")" << status.toStdString() << R"(",)";
			has_fields = true;
		}
	}

	if (has_fields) {
		std::string json_str = json_payload.str();
		if (json_str.back() == ',')
			json_str.pop_back();
		json_str += "}";
		qfDebug() << serviceName().toStdString() + " [onRunChanged/run] run_id:" << run_id;
		sendCompetitorUpdate(QString::fromStdString(json_str), run_id);
	}

	// Start time punched by a competitor of a class without drawn start times (start by punching
	// the start unit or a mass start). OFeed knows such start time from the punch only and it is
	// needed to compute the result, it is sent in a separate request, so that a rejected start
	// slot cannot drop the result sent above.
	if (start_time_ms > 0 && is_result) {
		qf::core::sql::Query dq;
		dq.exec("SELECT COALESCE(classdefs.startIntervalMin, 0) AS startIntervalMin "
				"FROM runs "
				"INNER JOIN competitors ON competitors.id = runs.competitorId "
				"LEFT JOIN relays ON relays.id = runs.relayId "
				"INNER JOIN classdefs ON (classdefs.classId = competitors.classId OR classdefs.classId = relays.classId) "
				"AND classdefs.stageId = runs.stageId "
				"WHERE runs.id=" QF_IARG(run_id), qf::core::Exception::Throw);
		if (dq.next() && dq.value("startIntervalMin").toInt() == 0) {
			std::stringstream spayload;
			spayload << R"({"origin":"IT","useExternalId":true,)"
					 << R"("startTime":")" << datetime_to_string(stage_start_date_time.addMSecs(start_time_ms)).toStdString() << R"(")"
					 << "}";
			qfDebug() << serviceName().toStdString() + " [onRunChanged/startTime] run_id:" << run_id;
			sendCompetitorUpdate(QString::fromStdString(spayload.str()), run_id);
		}
	}

	// Competitor fields
	bool name_changed = competitor_fields.contains("firstname") || competitor_fields.contains("lastname");
	bool registration_changed = competitor_fields.contains("registration");
	bool note_changed = competitor_fields.contains("note");
	bool licence_changed = competitor_fields.contains("licence");
	bool bib_changed = competitor_fields.contains("startnumber");
	bool class_changed = competitor_fields.contains("classid");
	if (name_changed || registration_changed || note_changed || licence_changed || bib_changed || class_changed) {
		qf::core::sql::Query cq;
		cq.exec("SELECT competitors.firstName, competitors.lastName, competitors.registration, competitors.note, "
				"competitors.licence, competitors.startNumber, "
				"classes.id AS classId, "
				"clubs.name AS organisationName, clubs.abbr AS organisationAbbr "
				"FROM runs "
				"INNER JOIN competitors ON competitors.id = runs.competitorId "
				"LEFT JOIN relays ON relays.id = runs.relayId "
				"INNER JOIN classes ON classes.id = competitors.classId OR classes.id = relays.classId "
				"LEFT JOIN clubs ON substr(competitors.registration, 1, 3) = clubs.abbr "
				"WHERE runs.id=" QF_IARG(run_id), qf::core::Exception::Throw);
		if (cq.next()) {
			std::stringstream cpayload;
			cpayload << R"({"origin":"IT","useExternalId":true,)";
			bool has_cfields = false;

			if (name_changed) {
				cpayload << R"("firstname":")" << cq.value("firstName").toString().toStdString() << R"(",)"
						 << R"("lastname":")" << cq.value("lastName").toString().toStdString() << R"(",)";
				has_cfields = true;
			}
			if (registration_changed) {
				const QString reg = cq.value("registration").toString();
				const QString org_abbr = cq.value("organisationAbbr").toString();
				const QString org = !org_abbr.isEmpty() ? cq.value("organisationName").toString() : reg.left(3);
				cpayload << R"("registration":")" << reg.toStdString() << R"(",)"
						 << R"("organisation":")" << org.toStdString() << R"(",)";
				has_cfields = true;
			}
			if (note_changed) {
				cpayload << R"("note":")" << cq.value("note").toString().toStdString() << R"(",)";
				has_cfields = true;
			}
			if (licence_changed) {
				const QString lic = cq.value("licence").toString();
				if (!lic.isEmpty()) {
					cpayload << R"("license":")" << lic.toStdString() << R"(",)";
					has_cfields = true;
				}
			}
			if (bib_changed) {
				const QVariant bib_v = cq.value("startNumber");
				if (!bib_v.isNull()) {
					cpayload << R"("bibNumber":)" << bib_v.toInt() << ",";
					has_cfields = true;
				}
			}
			// Always include classExternalId so a simultaneous class change is not lost
			{
				const int class_id = cq.value("classId").toInt();
				if (class_id > 0) {
					cpayload << R"("classExternalId":")" << class_id << R"(",)";
					has_cfields = true;
				}
			}

			if (has_cfields) {
				std::string cs = cpayload.str();
				if (cs.back() == ',') cs.pop_back();
				cs += "}";
				qfDebug() << serviceName().toStdString() + " [onRunChanged/competitor] run_id:" << run_id;
				sendCompetitorUpdate(QString::fromStdString(cs), run_id);
			}
		}
	}
}

QByteArray OFeedClient::zlibCompress(QByteArray data)
{
	QByteArray compressedData = qCompress(data);
	// remove 4-byte length header - leave only the raw zlib stream
	compressedData.remove(0, 4);
	return compressedData;
}

}
