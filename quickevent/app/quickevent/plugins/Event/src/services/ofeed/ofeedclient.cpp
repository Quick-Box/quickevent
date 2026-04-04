#include "ofeedclient.h"
#include "ofeedclientwidget.h"

#include "../../eventplugin.h"

#include <qf/gui/framework/mainwindow.h>
#include <qf/gui/dialogs/dialog.h>
#include <qf/core/log.h>
#include <qf/core/utils/htmlutils.h>

#include <qf/core/sql/connection.h>
#include <qf/core/sql/query.h>
#include <qf/core/sql/transaction.h>

#include <plugins/Runs/src/runsplugin.h>
#include <plugins/Relays/src/relaysplugin.h>
#include <plugins/Competitors/src/competitordocument.h>

#include <quickevent/core/si/checkedcard.h>
#include <quickevent/core/utils.h>

#include <QCoreApplication>
#include <QBuffer>
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

		QString stageConfigKey(const QString &prefix, const QString &suffix, int stage)
		{
			return prefix + QLatin1Char('.') + suffix + QStringLiteral(".E") + QString::number(stage);
		}
	}

OFeedClient::OFeedClient(QObject *parent)
	: Super(OFeedClient::serviceName(), parent)
{
	m_networkManager = new QNetworkAccessManager(this);
	m_exportTimer = new QTimer(this);
	connect(m_exportTimer, &QTimer::timeout, this, &OFeedClient::onExportTimerTimeOut);
	connect(this, &OFeedClient::settingsChanged, this, &OFeedClient::init, Qt::QueuedConnection);
	connect(getPlugin<EventPlugin>(), &Event::EventPlugin::dbEventNotify, this, &OFeedClient::onDbEventNotify, Qt::QueuedConnection);
}

QString OFeedClient::serviceName()
{
	return QStringLiteral("OFeed");
}

void OFeedClient::run()
{
	Super::run();
	ensureEventImageCachedAtStartup();
	exportStartListIofXml3([this]()
						   { exportResultsIofXml3(); });
	m_exportTimer->start();
}

void OFeedClient::stop()
{
	Super::stop();
	m_exportTimer->stop();
}

void OFeedClient::exportResultsIofXml3()
{
	int current_stage = getPlugin<EventPlugin>()->currentStageId();
	bool is_relays = getPlugin<EventPlugin>()->eventConfig()->isRelays();

	QString str = is_relays
					  ? getPlugin<RelaysPlugin>()->resultsIofXml30()
					  : getPlugin<RunsPlugin>()->resultsIofXml30Stage(current_stage);

	sendFile(tr("results upload"), "/rest/v1/upload/iof", str);
}

void OFeedClient::exportStartListIofXml3(std::function<void()> on_success)
{
	int current_stage = getPlugin<EventPlugin>()->currentStageId();
	bool is_relays = getPlugin<EventPlugin>()->eventConfig()->isRelays();

	QString str = is_relays
					  ? getPlugin<RelaysPlugin>()->startListIofXml30()
					  : getPlugin<RunsPlugin>()->startListStageIofXml30(current_stage, false);

	sendFile(tr("start list upload"), "/rest/v1/upload/iof", str, on_success);
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
	ensureEventImageCachedAtStartup();
}

void OFeedClient::onExportTimerTimeOut()
{
	// exportStartListIofXml3();
	if(runChangesProcessing()){
		getChangesByOrigin();
	}
	exportResultsIofXml3();
}

void OFeedClient::loadSettings()
{
	Super::loadSettings();
	init();
}

void OFeedClient::onDbEventNotify(const QString &domain, int connection_id, const QVariant &data)
{
	if (status() != Status::Running)
		return;
	Q_UNUSED(connection_id)

	// Handle read-out
	if (domain == QLatin1String(Event::EventPlugin::DBEVENT_CARD_PROCESSED_AND_ASSIGNED))
	{
		auto checked_card = quickevent::core::si::CheckedCard(data.toMap());
		int competitor_id = getPlugin<RunsPlugin>()->competitorForRun(checked_card.runId());
		qfInfo() << serviceName().toStdString() + "DB event competitor READ-OUT, competitor id: " << competitor_id << ", runs.id: " << checked_card.runId();
		onCompetitorReadOut(competitor_id);
	}

	// Handle edit competitor
	if (domain == QLatin1String(Event::EventPlugin::DBEVENT_COMPETITOR_EDITED))
	{
		int competitor_id = data.toInt();
		qfInfo() << serviceName().toStdString() + "DB event competitor EDITED, competitor id: " << competitor_id;
		onCompetitorEdited(competitor_id);
	}

	// Handle add competitor
	if (domain == QLatin1String(Event::EventPlugin::DBEVENT_COMPETITOR_ADDED))
	{
		if (isInsertFromOFeed)
		{
			qfWarning() << serviceName().toStdString() + " [new competitor]: added from OFeed, no need to send back as a new competitor from QE (already exists in OFeed)";
			// Set back default value
			isInsertFromOFeed = false;
		}
		else
		{
			int competitor_id = data.toInt();
			qfInfo() << serviceName().toStdString() + "DB event competitor ADDED, competitor id: " << competitor_id;
			onCompetitorAdded(competitor_id);
		}
	}

	// Handle delete competitor
	if (domain == QLatin1String(Event::EventPlugin::DBEVENT_COMPETITOR_DELETED))
	{
		int run_id = data.toInt();
		qfInfo() << serviceName().toStdString() + "DB event competitor DELETED, run id: " << run_id;
		sendCompetitorDeleted(run_id);
	}
}

QString OFeedClient::hostUrl() const
{
	int current_stage = getPlugin<EventPlugin>()->currentStageId();
	QString key = serviceName().toLower() + ".hostUrl.E" + QString::number(current_stage);
	return normalized_base_host_url(getPlugin<EventPlugin>()->eventConfig()->value(key, k_default_host_url).toString());
}

QString OFeedClient::eventId() const
{
	int current_stage = getPlugin<EventPlugin>()->currentStageId();
	return getPlugin<EventPlugin>()->eventConfig()->value(serviceName().toLower() + ".eventId.E" + QString::number(current_stage)).toString();
}

QString OFeedClient::eventPassword() const
{
	int current_stage = getPlugin<EventPlugin>()->currentStageId();
	return getPlugin<EventPlugin>()->eventConfig()->value(serviceName().toLower() + ".eventPassword.E" + QString::number(current_stage)).toString();
}

QString OFeedClient::changelogOrigin() const
{
	int current_stage = getPlugin<EventPlugin>()->currentStageId();
	QString key = serviceName().toLower() + ".changelogOrigin.E" + QString::number(current_stage);
    return getPlugin<EventPlugin>()->eventConfig()->value(key, "START").toString();
}

bool OFeedClient::isInsertFromOFeed = false;

QDateTime OFeedClient::lastChangelogCall() {
    int current_stage = getPlugin<EventPlugin>()->currentStageId();
    QString key = serviceName().toLower() + ".lastChangelogCall.E" + QString::number(current_stage);

    // Retrieve the stored value from the configuration
    QVariant value = getPlugin<EventPlugin>()->eventConfig()->value(key);

    // Check if the value exists
    if (!value.isValid() || value.toString().isEmpty()) {
        // No valid value exists, set the initial value
        QDateTime initialValue = QDateTime::fromSecsSinceEpoch(0); // Default to Unix epoch (1970-01-01T00:00:00Z)
        getPlugin<EventPlugin>()->eventConfig()->setValue(key, initialValue.toString(Qt::ISODate));
        getPlugin<EventPlugin>()->eventConfig()->save(serviceName().toLower());
        // qDebug() << "No lastChangelogCall found. Setting initial value to:" << initialValue.toString(Qt::ISODate);
        return initialValue;
    }

    // Convert the stored string to QDateTime
    QDateTime lastChangelog = QDateTime::fromString(value.toString(), Qt::ISODate);

    // Check if the conversion was successful
    if (!lastChangelog.isValid()) {
        // If invalid, set the default value
        QDateTime initialValue = QDateTime::fromSecsSinceEpoch(0); // Default to Unix epoch
        getPlugin<EventPlugin>()->eventConfig()->setValue(key, initialValue.toString(Qt::ISODate));
        getPlugin<EventPlugin>()->eventConfig()->save(serviceName().toLower());
        // qDebug() << "Invalid lastChangelogCall found. Setting initial value to:" << initialValue.toString(Qt::ISODate);
        return initialValue;
    }

    return lastChangelog;
}

bool OFeedClient::runXmlValidation()
{
	int current_stage = getPlugin<EventPlugin>()->currentStageId();
	QString key = serviceName().toLower() + ".runXmlValidation.E" + QString::number(current_stage);
	return getPlugin<EventPlugin>()->eventConfig()->value(key, "true").toBool();
}

bool OFeedClient::runChangesProcessing ()
{
	int current_stage = getPlugin<EventPlugin>()->currentStageId();
	QString key = serviceName().toLower() + ".runChangesProcessing.E" + QString::number(current_stage);
	return getPlugin<EventPlugin>()->eventConfig()->value(key, "false").toBool();
};

QString OFeedClient::receiptConfigKey(const QString &suffix) const
{
	const int current_stage = getPlugin<EventPlugin>()->currentStageId();
	return stageConfigKey(k_event_config_prefix, suffix, current_stage);
}

QVariant OFeedClient::receiptConfigValue(const QString &suffix, const QVariant &default_value) const
{
	return getPlugin<EventPlugin>()->eventConfig()->value(receiptConfigKey(suffix), default_value);
}

void OFeedClient::setReceiptConfigValue(const QString &suffix, const QVariant &value)
{
	auto *event_config = getPlugin<EventPlugin>()->eventConfig();
	event_config->setValue(receiptConfigKey(suffix), value);
	event_config->save(k_event_config_prefix);
}

bool OFeedClient::printEventImageOnReceipt() const
{
	return receiptConfigValue(QStringLiteral("receiptPrintEventImage"), false).toBool();
}

void OFeedClient::setPrintEventImageOnReceipt(bool on)
{
	setReceiptConfigValue(QStringLiteral("receiptPrintEventImage"), on);
}

bool OFeedClient::printEventQrCodeOnReceipt() const
{
	return receiptConfigValue(QStringLiteral("receiptPrintEventQrCode"), false).toBool();
}

void OFeedClient::setPrintEventQrCodeOnReceipt(bool on)
{
	setReceiptConfigValue(QStringLiteral("receiptPrintEventQrCode"), on);
}

int OFeedClient::receiptImageHeightMm() const
{
	bool ok = false;
	int height_mm = receiptConfigValue(QStringLiteral("receiptImageHeightMm"), 18).toInt(&ok);
	if(!ok)
		return 18;
	if(height_mm < 10)
		return 10;
	if(height_mm > 60)
		return 60;
	return height_mm;
}

void OFeedClient::setReceiptImageHeightMm(int height_mm)
{
	if(height_mm < 10)
		height_mm = 10;
	else if(height_mm > 60)
		height_mm = 60;
	setReceiptConfigValue(QStringLiteral("receiptImageHeightMm"), height_mm);
}

QString OFeedClient::receiptEventLinkUrl() const
{
	const QString custom_url = receiptConfigValue(QStringLiteral("receiptEventLinkUrl")).toString().trimmed();
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
	setReceiptConfigValue(QStringLiteral("receiptEventLinkUrl"), link_url);
}

QString OFeedClient::receiptEventQrCodeCaption() const
{
	return receiptConfigValue(QStringLiteral("receiptPrintEventQrCodeCaption"), defaultReceiptEventQrCodeCaption()).toString().trimmed();
}

QString OFeedClient::defaultReceiptEventQrCodeCaption() const
{
	return QStringLiteral("Live Results");
}

void OFeedClient::setReceiptEventQrCodeCaption(QString caption)
{
	caption = caption.trimmed();
	setReceiptConfigValue(QStringLiteral("receiptPrintEventQrCodeCaption"), caption);
}

bool OFeedClient::hasCachedEventImage() const
{
	return !cachedEventImageBase64().isEmpty();
}

QString OFeedClient::cachedEventImageBase64() const
{
	return receiptConfigValue(QStringLiteral("receiptImageDataBase64")).toString();
}

QString OFeedClient::cachedEventImageFormat() const
{
	return receiptConfigValue(QStringLiteral("receiptImageFormat"), QStringLiteral("png")).toString().toLower();
}

void OFeedClient::setCachedEventImage(const QByteArray &raw_data, const QString &format)
{
	auto *event_config = getPlugin<EventPlugin>()->eventConfig();
	event_config->setValue(receiptConfigKey(QStringLiteral("receiptImageDataBase64")), QString::fromLatin1(raw_data.toBase64()));
	event_config->setValue(receiptConfigKey(QStringLiteral("receiptImageFormat")), format.toLower());
	event_config->save(k_event_config_prefix);
}

void OFeedClient::clearCachedEventImage()
{
	auto *event_config = getPlugin<EventPlugin>()->eventConfig();
	event_config->setValue(receiptConfigKey(QStringLiteral("receiptImageDataBase64")), QString());
	event_config->setValue(receiptConfigKey(QStringLiteral("receiptImageFormat")), QString());
	event_config->save(k_event_config_prefix);
}

void OFeedClient::ensureEventImageCachedAtStartup()
{
	if(m_eventImageStartupAttempted)
		return;
	if(!printEventImageOnReceipt())
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
	int current_stage = getPlugin<EventPlugin>()->currentStageId();
	getPlugin<EventPlugin>()->eventConfig()->setValue(serviceName().toLower() + ".hostUrl.E" + QString::number(current_stage), hostUrl);
	getPlugin<EventPlugin>()->eventConfig()->save(serviceName().toLower());
	m_eventImageStartupAttempted = false;
}

void OFeedClient::setEventId(QString eventId)
{
	int current_stage = getPlugin<EventPlugin>()->currentStageId();
	getPlugin<EventPlugin>()->eventConfig()->setValue(serviceName().toLower() + ".eventId.E" + QString::number(current_stage), eventId);
	getPlugin<EventPlugin>()->eventConfig()->save(serviceName().toLower());
	m_eventImageStartupAttempted = false;
}

void OFeedClient::setEventPassword(QString eventPassword)
{
	int current_stage = getPlugin<EventPlugin>()->currentStageId();
	getPlugin<EventPlugin>()->eventConfig()->setValue(serviceName().toLower() + ".eventPassword.E" + QString::number(current_stage), eventPassword);
	getPlugin<EventPlugin>()->eventConfig()->save(serviceName().toLower());
	m_eventImageStartupAttempted = false;
}

void OFeedClient::setChangelogOrigin(QString changelogOrigin)
{
	int current_stage = getPlugin<EventPlugin>()->currentStageId();
	getPlugin<EventPlugin>()->eventConfig()->setValue(serviceName().toLower() + ".changelogOrigin.E" + QString::number(current_stage), changelogOrigin);
	getPlugin<EventPlugin>()->eventConfig()->save(serviceName().toLower());
}

void OFeedClient::setLastChangelogCall(QDateTime lastChangelogCall)
{
	int current_stage = getPlugin<EventPlugin>()->currentStageId();
	getPlugin<EventPlugin>()->eventConfig()->setValue(serviceName().toLower() + ".lastChangelogCall.E" + QString::number(current_stage), lastChangelogCall);
	getPlugin<EventPlugin>()->eventConfig()->save(serviceName().toLower());
}

void OFeedClient::setRunXmlValidation(bool runXmlValidation)
{
	int current_stage = getPlugin<EventPlugin>()->currentStageId();
	getPlugin<EventPlugin>()->eventConfig()->setValue(serviceName().toLower() + ".runXmlValidation.E" + QString::number(current_stage), runXmlValidation);
	getPlugin<EventPlugin>()->eventConfig()->save(serviceName().toLower());
}

void OFeedClient::setRunChangesProcessing(bool runChangesProcessing)
{
	int current_stage = getPlugin<EventPlugin>()->currentStageId();
	getPlugin<EventPlugin>()->eventConfig()->setValue(serviceName().toLower() + ".runChangesProcessing.E" + QString::number(current_stage), runChangesProcessing);
	getPlugin<EventPlugin>()->eventConfig()->save(serviceName().toLower());
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

	QUrl base_url(normalized_base_host_url(trimmed_host_url));
	if (!base_url.isValid() || base_url.scheme().isEmpty() || base_url.host().isEmpty()) {
		callback(false, tr("Invalid OFeed URL."));
		return;
	}

	base_url.setPath(QStringLiteral("/graphql"));

	QNetworkRequest request(base_url);
	request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");

	const QString combined = trimmed_event_id + ":" + trimmed_event_password;
	const QByteArray auth = "Basic " + combined.toUtf8().toBase64();
	request.setRawHeader("Authorization", auth);

	QJsonObject payload;
	payload["query"] = "query MyQuery($eventId: String!) { event(id: $eventId) { name organizer } }";
	QJsonObject variables;
	variables["eventId"] = trimmed_event_id;
	payload["variables"] = variables;

	QNetworkReply *reply = m_networkManager->post(request, QJsonDocument(payload).toJson(QJsonDocument::Compact));

	connect(reply, &QNetworkReply::finished, this, [reply, callback]() {
		const int http_status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
		const QString http_reason = reply->attribute(QNetworkRequest::HttpReasonPhraseAttribute).toString();
		const QByteArray response_bytes = reply->readAll();
		const QString response_text = QString::fromUtf8(response_bytes).trimmed();

		auto callback_error = [&](const QString &msg) {
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
			const QString reason = http_reason.isEmpty() ? tr("Unexpected HTTP response") : http_reason;
			callback_error(reason);
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

		const QJsonObject response_obj = doc.object();
		if (response_obj.contains("errors") && response_obj.value("errors").isArray()) {
			const QJsonArray errors = response_obj.value("errors").toArray();
			QString first_error = tr("GraphQL error");
			if (!errors.isEmpty() && errors.first().isObject()) {
				const QString message = errors.first().toObject().value("message").toString().trimmed();
				if (!message.isEmpty())
					first_error = message;
			}
			callback_error(first_error);
			reply->deleteLater();
			return;
		}

		const QJsonObject data_obj = response_obj.value("data").toObject();
		const QJsonObject event_obj = data_obj.value("event").toObject();
		if (event_obj.isEmpty()) {
			callback_error(tr("Missing event data in response."));
			reply->deleteLater();
			return;
		}

		const QString event_name = event_obj.value("name").toString().trimmed();
		if (event_name.isEmpty()) {
			callback_error(tr("Missing event name in response."));
			reply->deleteLater();
			return;
		}

		const QString organizer = event_obj.value("organizer").toString().trimmed();
		QString success_message = event_name;
		if (!organizer.isEmpty())
			success_message = QStringLiteral("%1 (%2)").arg(event_name, organizer);

		callback(true, success_message);
		reply->deleteLater();
	});
}

void OFeedClient::sendFile(QString name, QString request_path, QString file, std::function<void()> on_success)
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
		qDebug() << "Upload without IOF XML validation, validateXml: " + QString(xmlValidation ? "true" : "false");
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
	connect(reply, &QNetworkReply::finished, this, [reply, name, request_url, on_success]() {
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
	});
}

/// @brief Update competitors data by OFeed id or external id (QE id -> runs.id)
/// @param json_body body with the competitors data
/// @param competitor_or_external_id ofeed competitor id or external id (for QE runs.id)
/// @param using_external_id indicator which id is used - competitor (internal OFeed) or external (QE id from runs table)
void OFeedClient::sendCompetitorUpdate(QString json_body, int competitor_or_external_id, bool using_external_id = true)
{
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

	connect(reply, &QNetworkReply::finished, this, [=]()
			{
				if(reply->error()) {
					qfError() << serviceName().toStdString() + " [competitor update]: " << reply->errorString();
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
							qfInfo() << serviceName().toStdString() + " [competitor details update]: " << data_message;
						} else {
							qfInfo() << serviceName().toStdString() + " [competitor details update]: ok, but no data message found.";
						}
					} else {
						qfError() << serviceName().toStdString() + " [competitor details update] Unexpected response: " << response;
					}
				}
				reply->deleteLater(); });
}

void OFeedClient::sendCompetitorAdded(QString json_body)
{
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

	connect(reply, &QNetworkReply::finished, this, [=]()
			{
				if(reply->error()) {
					qfError() << serviceName().toStdString() + " [new competitor]: " << reply->errorString();
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
					qfError() << serviceName().toStdString() + " [deleted competitor]: " << reply->errorString();
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

void OFeedClient::getChangesByOrigin()
{
	try
	{
		QDateTime last_changelog_call_value = lastChangelogCall();
		QDateTime initial_value = QDateTime::fromSecsSinceEpoch(0); // Unix epoch

		QString graphQLquery = R"(
		query ChangelogByEvent($eventId: String!, $origin: String) {
			changelogByEvent(eventId: $eventId, origin: $origin) {
				id
				type
				previousValue
				newValue
				origin
				competitor {
					id
					externalId
					firstname
					lastname
				}
				createdAt
			}
		}
		)";

		QJsonObject variables;
		variables["eventId"] = eventId();
		variables["origin"] = changelogOrigin();

		// Check if last_changelog_call_value is valid/not default
		if (last_changelog_call_value != initial_value)
		{
			graphQLquery = R"(
			query ChangelogByEvent($eventId: String!, $origin: String, $since: DateTime) {
				changelogByEvent(eventId: $eventId, origin: $origin, since: $since) {
					id
					type
					previousValue
					newValue
					origin
					competitor {
						id
						externalId
						firstname
						lastname
					}
					createdAt
				}
			}
			)";

			variables["since"] = last_changelog_call_value.toString(Qt::ISODate);
		}

		sendGraphQLRequest(graphQLquery, variables, [this](QJsonObject data)
						   {
			if (!data.isEmpty())
			{
				// Check if the "data" key exists and is an array
				if (data.contains("changelogByEvent") && data["changelogByEvent"].isArray()) {
					QJsonArray changelog_array = data["changelogByEvent"].toArray();

					if (changelog_array.isEmpty()) {
						qfInfo() << "No changes from origin: " << changelogOrigin();
						return;
					}

					// Process the data
					processCompetitorsChanges(changelog_array);

					// Update last changelog call with the adjusted execution time
					QDateTime request_execution_time = QDateTime::currentDateTimeUtc();
					setLastChangelogCall(request_execution_time);
				}

			} }, true);
	}
	catch (const std::exception &e)
	{
		qCritical() << tr("Exception occurred while getting changes by origin: ") << e.what();
	}
}

void OFeedClient::processCompetitorsChanges(QJsonArray data_array)
{
	if (data_array.isEmpty())
	{
		return;
	}

	for (const QJsonValue &value : data_array)
	{

		if (!value.isObject())
		{
			continue;
		}

		QJsonObject change = value.toObject();

		// Extract values
		QString type = change["type"].toString();
		QString previous_value = change["previousValue"].toString();
		QString new_value = change["newValue"].toString();

		// Retrieve competitor and details
		auto competitor = change.value("competitor").toObject();
		int ofeed_competitor_id = competitor["id"].toInt();
		QString external_id_str = competitor["externalId"].toString();
		int runs_id = external_id_str.toInt();
		qfInfo() << "Processing change for competitorId (OFeed externalId):" << runs_id << ", type:" << type << ", " << previous_value << " -> " << new_value;

		// Handle each type of change
		if (type == "si_card_change")
		{
			processCardChange(runs_id, new_value);
		}
		else if (type == "status_change")
		{
			processStatusChange(runs_id, new_value);
		}
		else if (type == "note_change")
		{
			processNoteChange(runs_id, new_value);
		}
		else if (type == "competitor_create")
		{
			processNewRunner(ofeed_competitor_id);
		}
		else
		{
			qfWarning() << "Unsupported change type: " << type.toStdString();
			continue;
		}

		// Store the processed change
		storeChange(change);
	}
}

void OFeedClient::processCardChange(int runs_id, const QString &new_value)
{
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
	bool notStart = new_value == "DidNotStart" ? true : false;

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

void OFeedClient::processNewRunner(int ofeed_competitor_id)
{
	qDebug() << "Storing a new runner (OFeed id):" << ofeed_competitor_id;
	QString graphQLquery = R"(
	query CompetitorById($competitorByIdId: Int!) {
			competitorById(id: $competitorByIdId) {
				firstname
				lastname
				registration
				card
				note
				class {
					externalId
				}
			}
		}
		)";

	QJsonObject variables;
	variables["competitorByIdId"] = ofeed_competitor_id;
	sendGraphQLRequest(graphQLquery, variables, [this, ofeed_competitor_id](QJsonObject data)
	{
		if (!data.isEmpty())
		{
			QJsonObject competitor_by_id = data.value("competitorById").toObject();
			auto competitor_detail_class = competitor_by_id.value("class").toObject();
			// Create the competitor in QE
			Competitors::CompetitorDocument doc;
			doc.loadForInsert();
			doc.setValue("firstName", competitor_by_id.value("firstname").toString());
			doc.setValue("lastName", competitor_by_id.value("lastname").toString());
			doc.setValue("registration", competitor_by_id.value("registration").toString());
			doc.setValue("classid", competitor_detail_class.value("externalId").toString());
			doc.setSiid(competitor_by_id.value("card").toInt());
			doc.setValue("note", competitor_by_id.value("note").toString());

			// Change the flag to handle emited db event
			isInsertFromOFeed = true;

			// Save emits db event
			doc.save();

			auto competitor_id = doc.value("competitors.id");

			// Get runs.id for current stage
			int current_stage = getPlugin<EventPlugin>()->currentStageId();
			int run_id = doc.runsIds().value(current_stage - 1);

			// Update externalId at OFeed
			std::stringstream json_payload;
			json_payload << "{"
			<< R"("origin":"IT",)"
			<< R"("externalId":")" << run_id << R"(")"
			<< "}";

			std::string json_str = json_payload.str();

			// Convert std::string to QString
			QString json_body = QString::fromStdString(json_str);
			sendCompetitorUpdate(json_body, ofeed_competitor_id, false);
		}
		else
		{
			qfError() << tr("No data received or an error occurred.");
		}
	}, false);
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

	QJsonDocument change_json_doc(change);
	QString change_json = QString::fromUtf8(change_json_doc.toJson(QJsonDocument::Compact));

	int change_id = change["id"].toInt();
	auto created = QDateTime::fromString(change["createdAt"].toString(), Qt::ISODate);

	qf::core::sql::Query q;
	try
	{
		q.prepare("INSERT INTO qxchanges (data_type, data, orig_data, source, user_id, stage_id, change_id, created, status, status_message)"
				  " VALUES (:dataType, :data, :origData, :source, :userId, :stageId, :changeId, :created, :status, :statusMessage)");
		q.bindValue(":dataType", change["type"].toString());
		q.bindValue(":data", new_value);
		q.bindValue(":origData", previous_value);
		q.bindValue(":source", "OFeed");
		q.bindValue(":userId", competitor_id);
		q.bindValue(":stageId", current_stage);
		q.bindValue(":changeId", change_id);
		q.bindValue(":created", created);
		q.bindValue(":status", "Accepted");
		q.bindValue(":statusMessage", firstname + " " + lastname + ": " + previous_value + " -> " + new_value);
		if (!q.exec(qf::core::Exception::Throw))
		{
			qfError() << "Database query failed:" << q.lastError().text();
		}
	}
	catch (const std::exception &e)
	{
		qCritical() << "Exception occurred while executing query:" << e.what();
	}
	catch (...)
	{
		qCritical() << "Unknown exception occurred while executing query.";
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
	// Handle time initial value
	if (time == -1)
	{
		return "Inactive";
	}
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
	if (is_not_competing)
		return "NotCompeting";
	if (is_miss_punch)
		return "MissingPunch";
	if (is_did_not_finish)
		return "DidNotFinish";
	if (is_did_not_start)
		return "DidNotStart";
	if (is_bad_check || is_disq_by_organizer || is_disq)
		return "Disqualified";
	if (time)
		return "OK";   // OK
	return "Inactive"; // Inactive as default status
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
			json_payload << R"("time":)" << running_time << ",";
		}

		// Get the final JSON string
		std::string json_str = json_payload.str();

		// Remove the trailing comma if necessary
		if (json_str.back() == ',')
		{
			json_str.pop_back();
		}

		json_str += "}";

		// Output the JSON for debugging
		// qDebug() << serviceName().toStdString() + " - competitor added - json: " << json_str;

		// Convert std::string to QString
		QString json_qstr = QString::fromStdString(json_str);

		// Send
		sendCompetitorAdded(json_qstr);
	}
}

void OFeedClient::onCompetitorEdited(int competitor_id)
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
		QString note = q.value(QStringLiteral("note")).toString();

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
		json_payload << R"("useExternalId":true,)"
						 << R"("classExternalId":")" << class_id << R"(",)";

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
			json_payload << R"("time":)" << running_time << ",";
		}

		// Get the final JSON string
		std::string json_str = json_payload.str();

		// Remove the trailing comma if necessary
		if (json_str.back() == ',')
		{
			json_str.pop_back();
		}

		json_str += "}";

		// Output the JSON for debugging
		// qDebug() << serviceName().toStdString() + " - competitor edited - json: " << json_str;

		// Convert std::string to QString
		QString json_qstr = QString::fromStdString(json_str);

		// Send
		sendCompetitorUpdate(json_qstr, run_id);
	}
}

void OFeedClient::onCompetitorReadOut(int competitor_id)
{
	if (competitor_id == 0)
		return;

	int stage_id = getPlugin<EventPlugin>()->currentStageId();
	QDateTime stage_start_date_time = getPlugin<EventPlugin>()->stageStartDateTime(stage_id);
	qf::core::sql::Query q;
	q.exec("SELECT runs.id AS runId, "
		   "runs.disqualified, "
		   "runs.disqualifiedByOrganizer, "
		   "runs.misPunch, "
		   "runs.badCheck, "
		   "runs.notStart, "
		   "runs.notFinish, "
		   "runs.notCompeting, "
		   "runs.startTimeMs, "
		   "runs.finishTimeMs, "
		   "runs.timeMs, "
		   "competitors.note "
		   "FROM runs "
		   "INNER JOIN competitors ON competitors.id = runs.competitorId "
		   "LEFT JOIN relays ON relays.id = runs.relayId  "
		   "INNER JOIN classes ON classes.id = competitors.classId OR classes.id = relays.classId  "
		   "WHERE competitors.id=" QF_IARG(competitor_id) " AND runs.stageId=" QF_IARG(stage_id),
		   qf::core::Exception::Throw);
	if (q.next())
	{
		int run_id = q.value("runId").toInt();
		bool is_disq = q.value(QStringLiteral("disqualified")).toBool();
		bool is_disq_by_organizer = q.value(QStringLiteral("disqualifiedByOrganizer")).toBool();
		bool is_miss_punch = q.value(QStringLiteral("misPunch")).toBool();
		bool is_bad_check = q.value(QStringLiteral("badCheck")).toBool();
		bool is_did_not_start = q.value(QStringLiteral("notStart")).toBool();
		bool is_did_not_finish = q.value(QStringLiteral("notFinish")).toBool();
		bool is_not_competing = q.value(QStringLiteral("notCompeting")).toBool();
		int start_time = q.value(QStringLiteral("startTimeMs")).toInt();
		int finish_time = q.value(QStringLiteral("finishTimeMs")).toInt();
		int running_time = q.value(QStringLiteral("timeMs")).toInt();
		QString status = getIofResultStatus(running_time, is_disq, is_disq_by_organizer, is_miss_punch, is_bad_check, is_did_not_start, is_did_not_finish, is_not_competing);
		QString origin = "IT";
		QString note = "QE read-out, " + q.value(QStringLiteral("note")).toString();

		// Use std::stringstream to build the JSON string
		std::stringstream json_payload;
		json_payload << "{"
					 << R"("useExternalId":true,)"
					 << R"("origin":")" << origin.toStdString() << R"(",)"
					 << R"("startTime":")" << datetime_to_string(stage_start_date_time.addMSecs(start_time)).toStdString() << R"(",)"
					 << R"("finishTime":")" << datetime_to_string(stage_start_date_time.addMSecs(finish_time)).toStdString() << R"(",)"
					 << R"("time":)" << running_time / 1000 << ","
					 << R"("status":")" << status.toStdString() << R"(",)"
					 << R"("note":")" << note.toStdString() << R"(")"
					 << "}";

		// Get the final JSON string
		std::string json_str = json_payload.str();

		// Convert std::string to QString
		QString json_qstr = QString::fromStdString(json_str);

		sendCompetitorUpdate(json_qstr, run_id);
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
