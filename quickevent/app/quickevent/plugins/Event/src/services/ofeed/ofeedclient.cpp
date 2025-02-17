#include "ofeedclient.h"
#include "ofeedclientwidget.h"

#include "../../eventplugin.h"

#include <qf/qmlwidgets/framework/mainwindow.h>
#include <qf/qmlwidgets/dialogs/dialog.h>

#include <qf/core/log.h>
#include <plugins/Runs/src/runsplugin.h>
#include <plugins/Relays/src/relaysplugin.h>
#include <quickevent/core/si/checkedcard.h>
#include <qf/core/utils/htmlutils.h>
#include <qf/core/sql/query.h>

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QHttpPart>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QSettings>
#include <QStandardPaths>
#include <QTextStream>
#include <QTimer>
#include <regex>

namespace qfc = qf::core;
namespace qfw = qf::qmlwidgets;
namespace qfd = qf::qmlwidgets::dialogs;
namespace qfs = qf::core::sql;
using qf::qmlwidgets::framework::getPlugin;
using Event::EventPlugin;
using Relays::RelaysPlugin;
using Runs::RunsPlugin;

namespace Event {
namespace services {

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

void OFeedClient::run() {
	Super::run();
	exportStartListIofXml3([this](){exportResultsIofXml3();});
	m_exportTimer->start();
}

void OFeedClient::stop() {
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

	sendFile("results upload", "/rest/v1/upload/iof", str);
}

void OFeedClient::exportStartListIofXml3(std::function<void()> on_success)
{
	int current_stage = getPlugin<EventPlugin>()->currentStageId();
	bool is_relays = getPlugin<EventPlugin>()->eventConfig()->isRelays();

	QString str = is_relays
			? getPlugin<RelaysPlugin>()->startListIofXml30()
			: getPlugin<RunsPlugin>()->startListStageIofXml30(current_stage);

	sendFile("start list upload", "/rest/v1/upload/iof", str, on_success);
}

qf::qmlwidgets::framework::DialogWidget *OFeedClient::createDetailWidget()
{
    auto *w = new OFeedClientWidget();
	return w;
}

void OFeedClient::init()
{
    OFeedClientSettings ss = settings();
	m_exportTimer->setInterval(ss.exportIntervalSec() * 1000);
}

void OFeedClient::onExportTimerTimeOut()
{
	exportResultsIofXml3();
}

void OFeedClient::loadSettings()
{
	Super::loadSettings();
	init();
}

void OFeedClient::sendFile(QString name, QString request_path, QString file, std::function<void()> on_success) {
     // Create a multi-part request (like FormData in JS)
	QHttpMultiPart *multi_part = new QHttpMultiPart(QHttpMultiPart::FormDataType);

	// Prepare the Authorization header with Bearer token
	QString combined = eventId() + ":" + eventPassword();
    QByteArray base_64_auth = combined.toUtf8().toBase64();
	QString auth_value = "Bearer " + QString(base_64_auth);
	QByteArray auth_header = auth_value.toUtf8();

    // Add eventId field
	QHttpPart event_id_part;
	event_id_part.setHeader(QNetworkRequest::ContentDispositionHeader, QVariant("form-data; name=\"eventId\""));
	event_id_part.setBody(eventId().toUtf8());
	multi_part->append(event_id_part);

    // Add xml content with fake filename that must be present
	QHttpPart file_part;
    file_part.setHeader(QNetworkRequest::ContentDispositionHeader, QVariant("form-data; name=\"file\"; filename=\"uploaded_file.xml\""));
	file_part.setBody(file.toUtf8());
	multi_part->append(file_part);

    // Create network request with authorization header
	QUrl url(hostUrl() + request_path);
	QNetworkRequest request(url);
    request.setRawHeader("Authorization", auth_header);

    // Send request
	QNetworkReply *reply = m_networkManager->post(request, multi_part);
	multi_part->setParent(reply);

    // Cleanup
    connect(reply, &QNetworkReply::finished, this, [this, reply, name, on_success]()
	{
		if(reply->error()) {
			auto err_msg = hostUrl() + " [" + name + "]: ";
			auto response_body = reply->readAll();
			if (!response_body.isEmpty())
				err_msg += response_body + " | ";
			qfError() << err_msg + reply->errorString();
		}
		else {
			qfInfo() << hostUrl() + " [" + name + "]: success";
			if (on_success)
				on_success();
		}
		reply->deleteLater();
	});
}

void OFeedClient::onDbEventNotify(const QString &domain, int connection_id, const QVariant &data)
{
    if (status() != Status::Running)
        return;
    Q_UNUSED(connection_id)
    if(domain == QLatin1String(Event::EventPlugin::DBEVENT_CARD_PROCESSED_AND_ASSIGNED)) {
        auto checked_card = quickevent::core::si::CheckedCard(data.toMap());
        int competitor_id = getPlugin<RunsPlugin>()->competitorForRun(checked_card.runId());
        onCompetitorChanged(competitor_id);
    }
    if(domain == QLatin1String(Event::EventPlugin::DBEVENT_COMPETITOR_EDITED)) {
        int competitor_id = data.toInt();
        onCompetitorChanged(competitor_id);
    }
}

QString OFeedClient::hostUrl() const
{
    int current_stage = getPlugin<EventPlugin>()->currentStageId();
    return getPlugin<EventPlugin>()->eventConfig()->value("ofeed.hostUrl.E" + QString::number(current_stage)).toString();
}

QString OFeedClient::eventId() const
{
    int current_stage = getPlugin<EventPlugin>()->currentStageId();
    return getPlugin<EventPlugin>()->eventConfig()->value("ofeed.eventId.E" + QString::number(current_stage)).toString();
}

QString OFeedClient::eventPassword() const
{
    int current_stage = getPlugin<EventPlugin>()->currentStageId();
    return getPlugin<EventPlugin>()->eventConfig()->value("ofeed.eventPassword.E" + QString::number(current_stage)).toString();
}

void OFeedClient::setHostUrl(QString hostUrl)
{
    int current_stage = getPlugin<EventPlugin>()->currentStageId();
    getPlugin<EventPlugin>()->eventConfig()->setValue("ofeed.hostUrl.E" + QString::number(current_stage), hostUrl);
    getPlugin<EventPlugin>()->eventConfig()->save("ofeed");
}

void OFeedClient::setEventId(QString eventId)
{
    int current_stage = getPlugin<EventPlugin>()->currentStageId();
    getPlugin<EventPlugin>()->eventConfig()->setValue("ofeed.eventId.E" + QString::number(current_stage), eventId);
    getPlugin<EventPlugin>()->eventConfig()->save("ofeed");
}

void OFeedClient::setEventPassword(QString eventPassword)
{
    int current_stage = getPlugin<EventPlugin>()->currentStageId();
    getPlugin<EventPlugin>()->eventConfig()->setValue("ofeed.eventPassword.E" + QString::number(current_stage), eventPassword);
    getPlugin<EventPlugin>()->eventConfig()->save("ofeed");
}

void OFeedClient::sendCompetitorChange(QString xml) {
    QUrl url(hostUrl() + "/graphql");
    QNetworkRequest request(url);
    // request.setRawHeader("pwd", apiKey().toUtf8());
    request.setHeader( QNetworkRequest::ContentTypeHeader, "application/zlib" );
    QNetworkReply *reply = m_networkManager->post(request, zlibCompress(xml.toUtf8()));

    connect(reply, &QNetworkReply::finished, this, [=]()
    {
        if(reply->error()) {
            qfError() << "OFeed [competitor change]:" << reply->errorString();
        }
        else {
            QString msg = reply->readAll();
            if (msg.contains("BADPWD"))
                qfError() << "OFeed [competitor change]: Invalid API key";
            else if (!msg.contains("OK"))
                qfError() << "OFeed [competitor change]: Failed to process request";
        }
        reply->deleteLater();
    });
}

static void append_list(QVariantList &lst, const QVariantList &new_lst)
{
    lst.insert(lst.count(), new_lst);
}

static int mop_start(int runner_start_ms) {
    int stage_id = getPlugin<EventPlugin>()->currentStageId();
    QDateTime event_start = getPlugin<EventPlugin>()->stageStartDateTime(stage_id);
    QDateTime runner_start = event_start.addMSecs(runner_start_ms);
    return event_start.date().startOfDay().msecsTo(runner_start) / 100;
}

static int mop_run_status_code(
		int time,
		bool isDisq,
		bool isDisqByOrganizer,
		bool isMissPunch,
		bool isBadCheck,
		bool isDidNotStart,
		bool isDidNotFinish,
		bool isNotCompeting)
{
	if (isNotCompeting)
		return 99; // NP (Not participating)
	if (isMissPunch)
		return 3; //MP (Missing punch)
	if (isDidNotFinish)
		return 4; // DNF (Did not finish)
	if (isDidNotStart)
		return 20; // DNS (Did not start)
	if (isBadCheck || isDisqByOrganizer || isDisq)
		return 5; // DQ (Disqualified)
	if (time)
		return 1; // OK
	return 0; // Unknown
}


void OFeedClient::onCompetitorChanged(int competitor_id)
{
    if (competitor_id == 0)
        return;

    int stage_id = getPlugin<EventPlugin>()->currentStageId();
    qf::core::sql::Query q;
    q.exec("SELECT competitors.registration, "
           "competitors.startNumber, "
           "competitors.lastName || ' ' || competitors.firstName AS name, "
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
           "INNER JOIN classes ON classes.id = competitors.classId OR classes.id = relays.classId  "
           "WHERE competitors.id=" QF_IARG(competitor_id) " AND runs.stageId=" QF_IARG(stage_id), qf::core::Exception::Throw);
    if(q.next()) {
        int run_id = q.value("runId").toInt();
        int start_num = q.value("startNumber").toInt();
        QString name = q.value("name").toString();
        QString class_id = q.value("classId").toString();
        int card_num = q.value("siId").toInt();
        bool isDisq = q.value("disqualified").toBool();
        bool isDisqByOrganizer = q.value("disqualifiedByOrganizer").toBool();
        bool isMissPunch = q.value("misPunch").toBool();
        bool isBadCheck = q.value("badCheck").toBool();
        bool isDidNotStart = q.value("notStart").toBool();
        bool isDidNotFinish = q.value("notFinish").toBool();
        bool isNotCompeting = q.value("notCompeting").toBool();
        int start_time = q.value("startTimeMs").toInt();
        int finish_time = q.value("finishTimeMs").toInt();
        int running_time = q.value("timeMs").toInt();

        int status_code = mop_run_status_code(running_time, isDisq, isDisqByOrganizer, isMissPunch, isBadCheck, isDidNotStart, isDidNotFinish, isNotCompeting);

        QVariantMap competitor {
            {"stat", status_code},
            {"cls", class_id}
        };
        if (start_num != 0)
            competitor.insert("bib", start_num);
        if(start_time != 0)
            competitor.insert("st", mop_start(start_time));
        if(finish_time > start_time && running_time != 0)
            competitor.insert("rt", running_time / 100);


        QVariantList xml_root{"MOPDiff",
            QVariantMap {
                {"xmlns", "http://www.melin.nu/mop"},
                {"creator", QStringLiteral("QuickEvent %1").arg(QCoreApplication::applicationVersion())},
                {"createTime", QDateTime::currentDateTimeUtc().toString(Qt::ISODate)}
            }
        };
        QVariantList xml_competitor{"cmp",
            QVariantMap {
                {"id", run_id },
                {"card", card_num },
            },
            QVariantList {"base",
                competitor,
                name
            }
        };
        append_list(xml_root, xml_competitor);
        qf::core::utils::HtmlUtils::FromXmlListOptions opts;
        opts.setDocumentTitle("Competitor change");
        auto xml_paylaod = qf::core::utils::HtmlUtils::fromXmlList(xml_root, opts);
        sendCompetitorChange(xml_paylaod);
    }
}

QByteArray OFeedClient::zlibCompress(QByteArray data)
{
	QByteArray compressedData = qCompress(data);
	// strip the 4-byte length put on by qCompress
	// internally qCompress uses zlib
	compressedData.remove(0, 4);
	return compressedData;
}
}}
