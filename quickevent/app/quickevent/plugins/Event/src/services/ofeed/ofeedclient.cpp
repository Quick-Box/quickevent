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
	return getPlugin<EventPlugin>()->eventConfig()->value(key, "https://api.orienteerfeed.com").toString();
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

void OFeedClient::setHostUrl(QString hostUrl)
{
	int current_stage = getPlugin<EventPlugin>()->currentStageId();
	getPlugin<EventPlugin>()->eventConfig()->setValue(serviceName().toLower() + ".hostUrl.E" + QString::number(current_stage), hostUrl);
	getPlugin<EventPlugin>()->eventConfig()->save(serviceName().toLower());
}

void OFeedClient::setEventId(QString eventId)
{
	int current_stage = getPlugin<EventPlugin>()->currentStageId();
	getPlugin<EventPlugin>()->eventConfig()->setValue(serviceName().toLower() + ".eventId.E" + QString::number(current_stage), eventId);
	getPlugin<EventPlugin>()->eventConfig()->save(serviceName().toLower());
}

void OFeedClient::setEventPassword(QString eventPassword)
{
	int current_stage = getPlugin<EventPlugin>()->currentStageId();
	getPlugin<EventPlugin>()->eventConfig()->setValue(serviceName().toLower() + ".eventPassword.E" + QString::number(current_stage), eventPassword);
	getPlugin<EventPlugin>()->eventConfig()->save(serviceName().toLower());
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
		qDebug() << "Processing change for competitorId (OFeed externalId):" << runs_id << ", type:" << type << ", " << previous_value << " -> " << new_value;

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
		q.prepare("INSERT INTO qxchanges (data_type, data, orig_data, source, user_id, stage_id, change_id, created, status_message)"
				  " VALUES (:dataType, :data, :origData, :source, :userId, :stageId, :changeId, :created, :statusMessage)");
		q.bindValue(":dataType", change["type"].toString());
		q.bindValue(":data", new_value);
		q.bindValue(":origData", previous_value);
		q.bindValue(":source", change_json);
		q.bindValue(":userId", competitor_id);
		q.bindValue(":stageId", current_stage);
		q.bindValue(":changeId", change_id);
		q.bindValue(":created", created);
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
