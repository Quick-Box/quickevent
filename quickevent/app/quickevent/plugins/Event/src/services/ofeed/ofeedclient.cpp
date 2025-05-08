#include "ofeedclient.h"
#include "ofeedclientwidget.h"

#include "../../eventplugin.h"

#include <qf/qmlwidgets/framework/mainwindow.h>
#include <qf/qmlwidgets/dialogs/dialog.h>
#include <qf/core/log.h>
#include <qf/core/utils/htmlutils.h>

#include <qf/core/sql/connection.h>
#include <qf/core/sql/query.h>
#include <qf/core/sql/transaction.h>

#include <plugins/Runs/src/runsplugin.h>
#include <plugins/Relays/src/relaysplugin.h>

#include <quickevent/core/si/checkedcard.h>
#include <quickevent/core/utils.h>

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
#include <QDateTime>
#include <QTimeZone>
#include <regex>
#include <iostream>
#include <sstream>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSet>

namespace qfc = qf::core;
namespace qfw = qf::qmlwidgets;
namespace qfd = qf::qmlwidgets::dialogs;
namespace qfs = qf::core::sql;
using Event::EventPlugin;
using qf::qmlwidgets::framework::getPlugin;
using Relays::RelaysPlugin;
using Runs::RunsPlugin;

namespace Event
{
	namespace services
	{

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

		void OFeedClient::sendFile(QString name, QString request_path, QString file, std::function<void()> on_success)
		{
			// Create a multi-part request (like FormData in JS)
			QHttpMultiPart *multi_part = new QHttpMultiPart(QHttpMultiPart::FormDataType);

			// Prepare the Authorization header with Bearer token
			QString combined = eventId() + ":" + eventPassword();
			QByteArray base_64_auth = combined.toUtf8().toBase64();
			QString auth_value = "Basic " + QString(base_64_auth);
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
			QUrl request_url(hostUrl() + request_path);
			QNetworkRequest request(request_url);
			request.setRawHeader("Authorization", auth_header);

			// Send request
			QNetworkReply *reply = m_networkManager->post(request, multi_part);
			multi_part->setParent(reply);

			// Cleanup
			connect(reply, &QNetworkReply::finished, this, [this, reply, name, request_url, on_success]()
					{
		if(reply->error()) {
			auto err_msg = "OFeed [" + name + "] " + request_url.toString() + " : ";
			auto response_body = reply->readAll();
			if (!response_body.isEmpty())
				err_msg += response_body + " | ";
			qfError() << err_msg + reply->errorString();
		}
		else {
			qfInfo() << "OFeed [" + name + "]: ok";
			if (on_success)
				on_success();
		}
		reply->deleteLater(); });
		}

		void OFeedClient::onDbEventNotify(const QString &domain, int connection_id, const QVariant &data)
		{
			if (status() != Status::Running)
				return;
			Q_UNUSED(connection_id)
			if (domain == QLatin1String(Event::EventPlugin::DBEVENT_CARD_PROCESSED_AND_ASSIGNED))
			{
				auto checked_card = quickevent::core::si::CheckedCard(data.toMap());
				int competitor_id = getPlugin<RunsPlugin>()->competitorForRun(checked_card.runId());
				onCompetitorReadOut(competitor_id);
			}
			if (domain == QLatin1String(Event::EventPlugin::DBEVENT_COMPETITOR_EDITED))
			{
				// TODO: use event DBEVENT_COMPETITOR_COUNTS_CHANGED - edit + count change -> new or deleted
				int competitor_id = data.toInt();
				onCompetitorEdited(competitor_id);
			}
		}

		// void OFeedClient::getChangesFromStart(){
		// TODO
		// Get changes
		// Update the database with changed values
		// qf::core::sql::Query q(sqlModel()->connectionName());
		// 		q.prepare("UPDATE runs SET siId=:siId WHERE competitorId=:competitorId", qf::core::Exception::Throw);
		// 		q.bindValue(":competitorId", competitor_id);
		// 		q.bindValue(":siId", siid());
		// 		q.exec(qf::core::Exception::Throw);
		// https://github.com/Quick-Box/quickevent/blob/master/quickevent/app/quickevent/plugins/Event/src/eventplugin.cpp#L434
		// https://github.com/Quick-Box/quickevent/blob/master/quickevent/app/quickevent/plugins/Competitors/src/competitorwidget.cpp#L191
		// Save them in the config table
		// Display the processed records in the table
		// }

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

		void OFeedClient::sendCompetitorChange(QString json_body, int competitor_id)
		{
			// Prepare the Authorization header base64 username:password
			QString combined = eventId() + ":" + eventPassword();
			QByteArray base_64_auth = combined.toUtf8().toBase64();
			QString auth_value = "Basic " + QString(base_64_auth);
			QByteArray auth_header = auth_value.toUtf8();

			// Create the URL for the PUT request
			QUrl url(hostUrl() + "/rest/v1/events/" + eventId() + "/competitors/" + QString::number(competitor_id) + "/external-id");

			// Create the network request
			QNetworkRequest request(url);
			request.setRawHeader("Authorization", auth_header);
			request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");

			// Send request
			QNetworkReply *reply = m_networkManager->put(request, json_body.toUtf8());

			connect(reply, &QNetworkReply::finished, this, [=]()
					{
		if(reply->error()) {
			qfError() << "OFeed [competitor change]:" << reply->errorString();
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
					qfInfo() << "OFeed [competitor change]:" << data_message;
				} else {
					qfInfo() << "OFeed [competitor change]: ok, but no data message found.";
				}
			} else {
				qfError() << "OFeed [competitor change] Unexpected response:" << response;
			}
		}
		reply->deleteLater(); });
		}

		void OFeedClient::sendNewCompetitor(QString json_body)
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
			qfError() << "OFeed [new competitor]:" << reply->errorString();
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
					qfInfo() << "OFeed [new competitor]:" << data_message;
				} else {
					qfInfo() << "OFeed [new competitor]: ok, but no data message found.";
				}
			} else {
				qfError() << "OFeed [new competitor] Unexpected response:" << response;
			}
		}
		reply->deleteLater(); });
		}

		static QString getIofResultStatus(
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

		static QString datetime_to_string(const QDateTime &dt)
		{
			return quickevent::core::Utils::dateTimeToIsoStringWithUtcOffset(dt);
		}

		void OFeedClient::onCompetitorEdited(int competitor_id)
		{
			if (competitor_id == 0)
			{
				return;
			}
			bool update_competitor = true;
			// Handle a new competitor - use the value of the sequence and check current value?
			// TODO: fix schema name
			// qf::core::sql::Connection::forName();
			// qf::core::utils::ConfigCLIOptions
			// cliOptions()->eventName()
			qf::core::sql::Query q_schema;
			q_schema.exec("select current_schema", qf::core::Exception::Throw);
			if (q_schema.next())
			{
				QString qe_event_id = q_schema.value(0).toString();

				// Query actual competitor id
				qf::core::sql::Query q_seq_comp;
				q_seq_comp.exec("SELECT last_value FROM pg_sequences WHERE schemaname = '" + qe_event_id + "' AND sequencename = 'competitors_id_seq'", qf::core::Exception::Throw);
				if (q_seq_comp.next())
				{
					int actual_seq_competitors_value = q_seq_comp.value(QStringLiteral("last_value")).toInt();

					// Check if a new competitor was inserted
					// if (actual_seq_competitors_value == competitor_id && domain == QLatin1String(Event::EventPlugin::DBEVENT_COMPETITOR_COUNTS_CHANGED)){
					if (actual_seq_competitors_value == competitor_id)
					{
						update_competitor = false;
					}
				}
			}

			int INT_INITIAL_VALUE = -1;

			int stage_id = getPlugin<EventPlugin>()->currentStageId();
			QDateTime stage_start_date_time = getPlugin<EventPlugin>()->stageStartDateTime(stage_id);
			qf::core::sql::Query q;
			q.exec("SELECT competitors.registration, "
				   "competitors.startNumber, "
				   "competitors.firstName, "
				   "competitors.lastName, "
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
				QString registration = q.value(QStringLiteral("registration")).toString();
				QString first_name = q.value(QStringLiteral("firstName")).toString();
				QString last_name = q.value(QStringLiteral("lastName")).toString();
				int card_number = q.value(QStringLiteral("siId")).toInt();
				QString organisation_name = q.value(QStringLiteral("organisationName")).toString();
				QString organisation_abbr = q.value(QStringLiteral("organisationAbbr")).toString();
				QString organisation = !organisation_abbr.isEmpty() ? organisation_name : registration.left(3);
				int class_id = q.value(QStringLiteral("classId")).toInt();
				QString nationality = "";
				// int run_id = q.value("runId").toInt();
				QString origin = "IT";
				QString note = "Edited from Quickevent";

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

				if (!update_competitor)
				{
					status = "Inactive";
				}

				// Use std::stringstream to build the JSON string
				std::stringstream json_payload;

				// Setup common values
				json_payload << "{"
							 << "\"origin\":\"" << origin.toStdString() << "\","
							 << "\"firstname\":\"" << first_name.toStdString() << "\","
							 << "\"lastname\":\"" << last_name.toStdString() << "\","
							 << "\"registration\":\"" << registration.toStdString() << "\","
							 << "\"organisation\":\"" << organisation.toStdString() << "\","
							 << "\"status\":\"" << status.toStdString() << "\","
							 << "\"note\":\"" << note.toStdString() << "\",";

				if (nationality != "")
				{
					json_payload << "\"nationality\":\"" << nationality.toStdString() << "\",";
				}

				// New competitor
				if (!update_competitor)
				{
					json_payload << "\"classExternalId\":\"" << class_id << "\","
								 << "\"externalId\":\"" << competitor_id << "\",";
				}
				// Update existing competitor
				else
				{
					json_payload << "\"useExternalId\":true,"
								 << "\"classExternalId\":\"" << class_id << "\",";
				}
				// Card number - QE saves 0 for empty si card
				if (card_number != 0)
				{
					json_payload << "\"card\":" << card_number << ",";
				}

				// Finish time
				if (finish_time != INT_INITIAL_VALUE)
				{
					json_payload << "\"finishTime\":\"" << datetime_to_string(stage_start_date_time.addMSecs(finish_time)).toStdString() << "\",";
				}

				// Star time
				if (start_time != INT_INITIAL_VALUE)
				{
					json_payload << "\"startTime\":\"" << datetime_to_string(stage_start_date_time.addMSecs(start_time)).toStdString() << "\",";
				}

				// Start bib
				if (start_bib != INT_INITIAL_VALUE)
				{
					json_payload << "\"bibNumber\":" << start_bib << ",";
				}

				//  Competitor's time
				if (running_time != INT_INITIAL_VALUE)
				{
					json_payload << "\"time\":" << running_time << ",";
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
				std::cout << "JSON Payload: " << json_str << std::endl;

				// Convert std::string to QString
				QString json_qstr = QString::fromStdString(json_str);

				// Send only a new competitor with start time (E1 button click calls db event which should be skipped=start time is not set)
				if (!update_competitor && start_time != INT_INITIAL_VALUE)
				{
					sendNewCompetitor(json_qstr);
				}
				else if (update_competitor && start_time != INT_INITIAL_VALUE)
				{
					sendCompetitorChange(json_qstr, competitor_id);
				}
			}
		}

		void OFeedClient::onCompetitorReadOut(int competitor_id)
		{
			if (competitor_id == 0)
				return;

			int stage_id = getPlugin<EventPlugin>()->currentStageId();
			QDateTime stage_start_date_time = getPlugin<EventPlugin>()->stageStartDateTime(stage_id);
			qf::core::sql::Query q;
			q.exec("SELECT runs.disqualified, "
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
				   "WHERE competitors.id=" QF_IARG(competitor_id) " AND runs.stageId=" QF_IARG(stage_id),
				   qf::core::Exception::Throw);
			if (q.next())
			{
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
				QString note = "Quickevent read-out ";

				// Use std::stringstream to build the JSON string
				std::stringstream json_payload;
				json_payload << "{"
							 << "\"useExternalId\":true,"
							 << "\"origin\":\"" << origin.toStdString() << "\","
							 << "\"startTime\":\"" << datetime_to_string(stage_start_date_time.addMSecs(start_time)).toStdString() << "\","
							 << "\"finishTime\":\"" << datetime_to_string(stage_start_date_time.addMSecs(finish_time)).toStdString() << "\","
							 << "\"time\":" << running_time / 1000 << ","
							 << "\"status\":\"" << status.toStdString() << "\","
							 << "\"note\":\"" << note.toStdString() << "\""
							 << "}";

				// Get the final JSON string
				std::string json_str = json_payload.str();

				// Output the JSON for debugging
				std::cout << "JSON Payload: " << json_str << std::endl;

				// Convert std::string to QString
				QString json_qstr = QString::fromStdString(json_str);

				sendCompetitorChange(json_qstr, competitor_id);
			}
		}
	}
}