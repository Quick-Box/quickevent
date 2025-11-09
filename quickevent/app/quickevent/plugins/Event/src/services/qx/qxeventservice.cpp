#include "qxeventservice.h"
#include "qxeventservicewidget.h"
#include "nodes.h"
#include "sqlapinode.h"

#include "../../eventplugin.h"
#include "../../../../Runs/src/runsplugin.h"

#include <qf/gui/framework/mainwindow.h>
#include <qf/core/log.h>
#include <qf/core/sql/query.h>
#include <qf/core/sql/connection.h>

#include <shv/iotqt/rpc/deviceconnection.h>
#include <shv/iotqt/node/shvnodetree.h>
#include <shv/iotqt/rpc/rpccall.h>

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
#include <QJsonDocument>
#include <QJsonArray>
#include <QUrlQuery>
#include <QSqlField>

using namespace shv::chainpack;
using namespace shv::iotqt::rpc;
using namespace shv::iotqt::node;

using namespace qf::core;
using namespace qf::gui;
using namespace qf::gui::dialogs;
using namespace qf::core::sql;
using qf::gui::framework::getPlugin;
using Event::EventPlugin;
using Runs::RunsPlugin;

namespace Event::services::qx {

//===============================================
// QxClientService
//===============================================
QxEventService::QxEventService(QObject *parent)
	: Super(QxEventService::serviceId(), parent)
	, m_rootNode(new shv::iotqt::node::ShvRootNode(this))
{
	auto *event_plugin = getPlugin<EventPlugin>();
	// connect(event_plugin, &Event::EventPlugin::dbEventNotify, this, &QxEventService::onDbEventNotify, Qt::QueuedConnection);

	new DotAppNode(m_rootNode);
	connect(event_plugin, &EventPlugin::eventOpenChanged, this, [this](bool is_open) {
		if (is_open) {
			new SqlApiNode(m_rootNode);
			auto *event = new EventNode(m_rootNode);
			auto *current_stage = new shv::iotqt::node::ShvNode("currentStage", event);
			new CurrentStageConfigNode(current_stage);
			//new CurrentStageStartListNode(current_stage);
			new CurrentStageRunsNode(current_stage);
			new CurrentStageClassesNode(current_stage);
		}
		else {
			qDeleteAll(m_rootNode->findChildren<EventNode*>());
		}
	});

	connect(m_rootNode, &shv::iotqt::node::ShvNode::sendRpcMessage, this, &QxEventService::sendRpcMessage);
}

QString QxEventService::serviceDisplayName() const
{
	return tr("QX Event");
}

QString QxEventService::serviceId()
{
	return QStringLiteral("qx");
}

void QxEventService::run() {
	using namespace shv::iotqt::rpc;

	auto ss = settings();

	delete m_rpcConnection;
	m_rpcConnection = new DeviceConnection(this);
	m_rpcConnection->setConnectionString(ss.shvBrokerUrl());
	RpcValue::Map opts;
	RpcValue::Map device;
	device["mountPoint"] = "test/quickbox";
	opts["device"] = device;
	m_rpcConnection->setConnectionOptions(opts);

	connect(m_rpcConnection, &ClientConnection::brokerConnectedChanged, this, &QxEventService::onBrokerConnectedChanged);
	connect(m_rpcConnection, &ClientConnection::socketError, this, &QxEventService::onBrokerSocketError);
	connect(m_rpcConnection, &ClientConnection::brokerLoginError, this, &QxEventService::onBrokerLoginError);
	connect(m_rpcConnection, &ClientConnection::rpcMessageReceived, this, &QxEventService::onRpcMessageReceived);

	m_rpcConnection->open();

//	connect(reply, &QNetworkReply::finished, this, [this, reply, ss]() {
//		if (reply->error() == QNetworkReply::NetworkError::NoError) {
//			auto data = reply->readAll();
//			auto doc = QJsonDocument::fromJson(data);
//			EventInfo event_info(doc.toVariant().toMap());
//			setStatusMessage(event_info.name() + (event_info.stage_count() > 1? QStringLiteral(" E%1").arg(event_info.stage()): QString()));
//			m_eventId = event_info.id();
//			connectToSSE(m_eventId);
//			if (!m_pollChangesTimer) {
//				m_pollChangesTimer = new QTimer(this);
//				connect(m_pollChangesTimer, &QTimer::timeout, this, &QxClientService::pollQxChanges);
//			}
//			pollQxChanges();
//			m_pollChangesTimer->start(10000);
//			Super::run();
//		}
//		else {
//			qfWarning() << "Cannot run QX service, network error:" << reply->errorString();
//		}
//	});
}

void QxEventService::stop()
{
	disconnectSSE();
	if (m_pollChangesTimer) {
		m_pollChangesTimer->stop();
	}
	Super::stop();
}

qf::gui::framework::DialogWidget *QxEventService::createDetailWidget()
{
	auto *w = new QxEventServiceWidget();
	return w;
}

void QxEventService::loadSettings()
{
	Super::loadSettings();
	auto ss = settings();
	if (ss.shvBrokerUrl().isEmpty()) {
		ss.setShvBrokerUrl("tcp://localhost?user=test&password=test");
	}
	m_settings = ss;
}

void QxEventService::onDbEventNotify(const QString &domain, int connection_id, const QVariant &data)
{
	Q_UNUSED(connection_id)
	Q_UNUSED(data)
	if (!isRunning()) {
		return;
	}
	if(domain == QLatin1String(Event::EventPlugin::DBEVENT_CARD_PROCESSED_AND_ASSIGNED)) {
		//auto checked_card = quickevent::core::si::CheckedCard(data.toMap());
		//int competitor_id = getPlugin<RunsPlugin>()->competitorForRun(checked_card.runId());
		//onCompetitorChanged(competitor_id);
	}
	else if(domain == QLatin1String(Event::EventPlugin::DBEVENT_COMPETITOR_EDITED)) {
		//int competitor_id = data.toInt();
		//onCompetitorChanged(competitor_id);
	}
	else if (domain == Event::EventPlugin::DBEVENT_RUN_CHANGED) {
		auto lst = data.toList();
		auto run_id = lst.value(0).toInt();
		auto ei = eventInfo();
		bool this_stage = false;
		qf::core::sql::Query q;
		q.execThrow(QStringLiteral("SELECT COUNT(*) FROM runs WHERE id=%1 AND stageId=%2").arg(run_id).arg(ei.stage()));
		if (q.next()) {
			this_stage = q.value(0).toInt() == 1;
		}
		if (!this_stage) {
			return;
		}
		auto qe_run_rec = lst.value(1).toMap();
		auto qx_run_rec = QVariantMap();
		auto remap_key = [&qe_run_rec, &qx_run_rec](const QString &qe_key, const QString &qx_key) {
			if (qe_run_rec.contains(qe_key)) {
				qx_run_rec[qx_key] = qe_run_rec.value(qe_key);
			}
		};

		qfInfo() << "DBEVENT_RUN_CHANGED run id:" << run_id << "data:" << QString::fromUtf8(QJsonDocument::fromVariant(qe_run_rec).toJson(QJsonDocument::Compact));

		remap_key("runs.siId", "si_id");
		remap_key("competitors.registration", "registration");
		if (!qx_run_rec.isEmpty()) {
			httpPostJson( "/api/event/current/changes/run-updated", QStringLiteral("run_id=%1").arg(run_id), qx_run_rec);
		}
	}
}

QNetworkAccessManager *QxEventService::networkManager()
{
	if (!m_networkManager) {
		m_networkManager = new QNetworkAccessManager(this);
	}
	return m_networkManager;
}

QNetworkReply *QxEventService::getRemoteEventInfo(const QString &qxhttp_host, const QString &api_token)
{
	auto *nm = networkManager();
	QNetworkRequest request;
	QUrl url(qxhttp_host);
	url.setPath("/api/event/current");
	request.setUrl(url);
	request.setRawHeader(QX_API_TOKEN, api_token.toUtf8());
	return nm->get(request);
}

QNetworkReply *QxEventService::postEventInfo(const QString &qxhttp_host, const QString &api_token)
{
	auto *nm = networkManager();
	QNetworkRequest request;
	QUrl url(qxhttp_host);
	// qfInfo() << "url " << url.toString();
	url.setPath("/api/event/current");
	// qfInfo() << "GET " << url.toString();
	request.setUrl(url);
	request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
	request.setRawHeader(QX_API_TOKEN, api_token.toUtf8());
	auto ei = eventInfo();
	auto data = QJsonDocument::fromVariant(ei).toJson();
	return nm->post(request, data);
}

void QxEventService::postStartListIofXml3(QObject *context, std::function<void (QString)> call_back)
{
	auto *ep = getPlugin<EventPlugin>();
	int current_stage = ep->currentStageId();
	bool is_relays = ep->eventConfig()->isRelays();
	if (!is_relays) {
		auto xml = getPlugin<RunsPlugin>()->startListStageIofXml30(current_stage);
		uploadSpecFile(SpecFile::StartListIofXml3, xml.toUtf8(), context, call_back);
	}
}

void QxEventService::postRuns(QObject *context, std::function<void (QString)> call_back)
{
	auto *ep = getPlugin<EventPlugin>();
	int current_stage = ep->currentStageId();
	bool is_relays = ep->eventConfig()->isRelays();
	if (!is_relays) {
		auto runs = getPlugin<RunsPlugin>()->qxExportRunsCsvJson(current_stage);
		auto json = qf::core::Utils::qvariantToJsonUtf8(runs, false);
		uploadSpecFile(SpecFile::RunsCsvJson, json, context, call_back);
	}
}

void QxEventService::getHttpJson(const QString &path, const QUrlQuery &query, QObject *context, const std::function<void (QVariant, QString)> &call_back)
{
	auto url = shvBrokerUrl();
	url.setPath(path);
	url.setQuery(query);
	// qfInfo() << url.toString();
	QNetworkRequest request;
	request.setUrl(url);
	auto *reply = networkManager()->get(request);
	// connect(reply, &QNetworkReply::finished, reply, &QNetworkReply::deleteLater);
	connect(reply, &QNetworkReply::finished, context, [call_back, reply]() {
		if (reply->error() == QNetworkReply::NetworkError::NoError) {
			QJsonParseError err;
			auto data = reply->readAll();
			auto json = QJsonDocument::fromJson(data, &err).toVariant();
			if (err.error != QJsonParseError::NoError) {
				call_back({}, err.errorString());
			}
			else {
				call_back(json, {});
			}
		}
		else {
			call_back({}, reply->errorString());
		}
		reply->deleteLater();
	});
}

QNetworkReply* QxEventService::getQxChangesReply(int from_id)
{
	auto url = shvBrokerUrl();

	url.setPath(QStringLiteral("/api/event/%1/changes").arg(eventId()));
	url.setQuery(QStringLiteral("from_id=%1").arg(from_id));
	qfInfo() << url.toString();
	QNetworkRequest request;
	request.setUrl(url);
	return networkManager()->get(request);
}

int QxEventService::eventId() const
{
	if (m_eventId == 0) {
		throw qf::core::Exception(tr("Event ID is not loaded, service is not probably running."));
	}
	return m_eventId;
}

QByteArray QxEventService::apiToken() const
{
	// API token must not be cached to enable service point
	// always to current stage event on qxhttpd
	auto *event_plugin = getPlugin<EventPlugin>();
	auto current_stage = event_plugin->currentStageId();
	return event_plugin->stageData(current_stage).qxApiToken().toUtf8();
}

QUrl QxEventService::shvBrokerUrl() const
{
	auto ss = settings();
	return QUrl(ss.shvBrokerUrl());
}

void QxEventService::postFileCompressed(std::optional<QString> path, std::optional<QString> name, QByteArray data, QObject *context , std::function<void (QString)> call_back)
{
	auto url = shvBrokerUrl();

	url.setPath(path.value_or("/api/event/current/file"));
	if (name.has_value()) {
		url.setQuery(QStringLiteral("name=%1").arg(name.value()));
	}
	QNetworkRequest request;
	request.setUrl(url);
	request.setRawHeader(QX_API_TOKEN, apiToken());
	request.setHeader(QNetworkRequest::ContentTypeHeader, QVariant("application/zip"));
	auto zdata = zlibCompress(data);
	QNetworkReply *reply = networkManager()->post(request, zdata);

	connect(reply, &QNetworkReply::finished, context, [reply, name, call_back]() {
		QString err;
		if(reply->error()) {
			err = reply->errorString();
			qfWarning() << "Post file:" << name.value_or("SPEC") << "error:" << err;
		}
		if (call_back) {
			call_back(err);
		}
		reply->deleteLater(); // should be called by Qt anyway
	});
}

void QxEventService::uploadSpecFile(SpecFile file, QByteArray data, QObject *context, const std::function<void (QString)> &call_back)
{
	switch (file) {
	case SpecFile::StartListIofXml3:
		postFileCompressed({}, "startlist-iof3.xml", data, context, call_back);
		break;
	case SpecFile::RunsCsvJson:
		postFileCompressed({}, "runs.csv.json", data, context, call_back);
		break;
	}
}

QByteArray QxEventService::zlibCompress(QByteArray data)
{
	QByteArray compressedData = qCompress(data);
	// strip the 4-byte length put on by qCompress
	// internally qCompress uses zlib
	compressedData.remove(0, 4);
	return compressedData;
}

void QxEventService::httpPostJson(const QString &path, const QString &query, QVariantMap json, QObject *context, const std::function<void (QString)> &call_back)
{
	if (!isRunning()) {
		return;
	}
	auto url = shvBrokerUrl();

	url.setPath(path);
	url.setQuery(query);

	QNetworkRequest request;
	request.setUrl(url);
	request.setRawHeader(QX_API_TOKEN, apiToken());
	request.setHeader(QNetworkRequest::ContentTypeHeader, QVariant("application/json"));
	auto data = QJsonDocument::fromVariant(json).toJson(QJsonDocument::Compact);
	qfInfo() << "HTTP POST JSON:" << url.toString() << "data:" << QString::fromUtf8(data);
	QNetworkReply *reply = networkManager()->post(request, data);
	if (context) {
		connect(reply, &QNetworkReply::finished, context, [reply, url, call_back]() {
			QString err;
			if(reply->error()) {
				err = reply->errorString();
				qfWarning() << "HTTP POST:" << url.toString() << "error:" << err;
			}
			if (call_back) {
				call_back(err);
			}
			reply->deleteLater();
		});
	}
	else {
		connect(reply, &QNetworkReply::finished, [reply, url, call_back]() {
			if(reply->error()) {
				qfWarning() << "HTTP POST:" << url.toString() << "error:" << reply->errorString();
			}
			reply->deleteLater();
		});
	}
}

void QxEventService::connectToSSE(int event_id)
{
	Q_UNUSED(event_id);
	// auto url = exchangeServerUrl();
	// url.setPath(QStringLiteral("/api/event/%1/run/changes/sse").arg(event_id));
	// QNetworkRequest request(url);
	// request.setRawHeader(QByteArray("Accept"), QByteArray("text/event-stream"));
	// request.setHeader(QNetworkRequest::UserAgentHeader, "QuickEvent");
	// request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);
	// request.setAttribute(QNetworkRequest::CacheLoadControlAttribute, QNetworkRequest::AlwaysNetwork); // Events shouldn't be cached

	// qfInfo() << "Connecting to SSE:" << url.toString();
	// m_replySSE = networkManager()->get(request);
	// qfInfo() << "Connected";
	// connect(m_replySSE, &QNetworkReply::readyRead, this, [this]() {
	// 	auto data = m_replySSE->readAll();
	// 	qfInfo() << "DATA:" << data.toStdString();
	// });
	// connect(m_replySSE, &QNetworkReply::finished, this, [this]() {
	// 	qfInfo() << "SSE finished:" << m_replySSE->errorString();
	// });
}

void QxEventService::disconnectSSE()
{
	if (m_replySSE) {
		qfInfo() << "Disconnecting SSE:" << m_replySSE;
		m_replySSE->deleteLater();
		m_replySSE = nullptr;
	}
}

void QxEventService::pollQxChanges()
{
	auto event_plugin = getPlugin<EventPlugin>();
	if(!getPlugin<EventPlugin>()->isEventOpen()) {
		return;
	}
	int stage_id = event_plugin->currentStageId();
	try {
		int max_change_id = 0;
		qf::core::sql::Query q;
		q.execThrow("SELECT MAX(change_id) FROM qxchanges WHERE stage_id=" + QString::number(event_plugin->currentStageId()));
		if (q.next()) {
			max_change_id = q.value(0).toInt();
		}
		auto *reply = getQxChangesReply(max_change_id + 1);
		connect(reply, &QNetworkReply::finished, this, [reply, stage_id]() {
			QString err;
			if(reply->error()) {
				err = reply->errorString();
				qfWarning() << "Load qxchanges error:" << err;
			}
			else {
				QJsonParseError err;
				auto data = reply->readAll();
				auto json = QJsonDocument::fromJson(data, &err);
				if (err.error != QJsonParseError::NoError) {
					qfWarning() << "Parse qxchanges error:" << err.errorString();
				}
				else {
					auto records = json.array().toVariantList();
					qf::core::sql::Query q;
					q.prepare("INSERT INTO qxchanges (data_type, data, data_id, source, user_id, status, status_message, stage_id, change_id, created)"
							  " VALUES (:data_type, :data, :data_id, :source, :user_id, :status, :status_message, :stage_id, :change_id, :created)"
							  " RETURNING id");
					for (const auto &v : records) {
						auto rec = v.toMap();
						auto ba = QJsonDocument::fromVariant(rec.value("data")).toJson(QJsonDocument::Compact);
						auto data = QString::fromUtf8(ba);
						q.bindValue(":data_type", rec.value("data_type"));
						q.bindValue(":data", data);
						q.bindValue(":data_id", rec.value("data_id"));
						q.bindValue(":source", rec.value("source"));
						q.bindValue(":user_id", rec.value("user_id"));
						q.bindValue(":status", rec.value("status"));
						q.bindValue(":status_message", rec.value("status_message"));
						q.bindValue(":stage_id", stage_id);
						auto change_id = rec.value("id").toInt();
						Q_ASSERT(change_id > 0);
						q.bindValue(":change_id", change_id);
						auto created = QDateTime::fromString(rec.value("created").toString(), Qt::ISODate);
						qfDebug() << "created:" << created.toString(Qt::ISODate);
						q.bindValue(":created", created);
						// may fail on prikey violation if more clients are inserting simultaneously
						if (q.exec()) {
							if (q.next()) {
								auto id = q.value(0).toInt();
								qfDebug() << "insert ID:" << id;
								getPlugin<EventPlugin>()->emitDbEvent(Event::EventPlugin::DBEVENT_QX_CHANGE_RECEIVED, id, true);
							}
						}
						else {
							qfInfo() << "sql error:" << q.lastErrorText();

						}
					}
				}
			}
			reply->deleteLater();
		});
	}
	catch (const qf::core::Exception &e) {
		qfWarning() << "Load qxchanges error:" << e.message();
	}
}

EventInfo QxEventService::eventInfo() const
{
	auto *event_plugin = getPlugin<EventPlugin>();
	auto *event_config = event_plugin->eventConfig();
	EventInfo ei;
	ei.set_stage(event_plugin->currentStageId());
	ei.set_stage_count(event_plugin->stageCount());
	ei.set_name(event_config->eventName());
	ei.set_place(event_config->eventPlace());
	ei.set_start_time(event_plugin->stageStartDateTime(event_plugin->currentStageId()).toString(Qt::ISODate));

	qf::core::sql::Query q;
	q.execThrow(QStringLiteral("SELECT classes.name AS name, COUNT(codes.id) AS control_count, length, climb, startTimeMin AS start_time, startIntervalMin as interval, lastStartTimeMin AS start_slot_count"
							   " FROM classes"
							   " LEFT JOIN classdefs ON classes.id=classdefs.classId AND classdefs.stageId=%1"
							   " LEFT JOIN courses ON classdefs.courseId=courses.id"
							   " LEFT JOIN coursecodes ON coursecodes.courseId=courses.id"
							   " LEFT JOIN codes ON coursecodes.codeId=codes.id AND codes.code>=%2 AND codes.code<=%3"
							   " GROUP BY classes.id, classes.name, length, climb, startTimeMin, startIntervalMin, lastStartTimeMin"
							   " ORDER BY classes.name")
				.arg(ei.stage())
				.arg(quickevent::core::CodeDef::PUNCH_CODE_MIN)
				.arg(quickevent::core::CodeDef::PUNCH_CODE_MAX)
				);

	QVariantList classes;
	{
		// QStringList columns{"name", "control_count", "length", "climb", "start_time", "interval", "start_slot_count"};
		QStringList columns;
		auto rec = q.record();
		for (auto i = 0; i < rec.count(); ++i) {
			columns << rec.field(i).name();
		}
		classes.insert(classes.length(), columns);
	}
	while (q.next()) {
		QVariantList values;
		auto rec = q.record();
		for (auto i = 0; i < rec.count(); ++i) {
			values << q.value(i);
		}
		auto interval = q.value("interval").toInt();
		if (interval > 0) {
			auto start_time = q.value("start_time").toInt();
			auto last_time = q.value("start_slot_count").toInt();
			auto start_slot_count = 1 + ((last_time - start_time) / interval);
			values.last() = start_slot_count;
		}
		else {
			values.last() = 0;
		}
		classes.insert(classes.length(), values);
	}
	ei.set_classes(classes);
	// qfInfo() << qf::core::Utils::qvariantToJson(ei, false);
	return ei;
}
//namespace {
//auto query_to_json_csv(QSqlQuery &q)
//{
//	QVariantList csv;
//	{
//		// QStringList columns{"name", "control_count", "length", "climb", "start_time", "interval", "start_slot_count"};
//		QStringList columns;
//		auto rec = q.record();
//		for (auto i = 0; i < rec.count(); ++i) {
//			columns << rec.field(i).name();
//		}
//		csv.insert(csv.length(), columns);
//	}
//	while (q.next()) {
//		QVariantList values;
//		auto rec = q.record();
//		for (auto i = 0; i < rec.count(); ++i) {
//			values << q.value(i);
//		}
//		csv.insert(csv.length(), values);
//	}
//	return csv;
//}
//}
int QxEventService::currentConnectionId()
{
	return qf::core::sql::Connection::forName().connectionId();
}

void QxEventService::onBrokerConnectedChanged(bool is_connected)
{
	if(is_connected) {
		setStatus(Status::Running);
		QTimer::singleShot(0, this, [this]() {
			subscribeChanges();
//			testRpcCall();
		});
	} else {
		setStatus(Status::Stopped);
	}

}

void QxEventService::onBrokerSocketError(const QString &err)
{
	setStatusMessage(tr("Broker socket error: %1").arg(err));
}

void QxEventService::onBrokerLoginError(const shv::chainpack::RpcError &err)
{
	setStatusMessage(tr("Broker login error: %1").arg(err.toString()));
}

void QxEventService::onRpcMessageReceived(const shv::chainpack::RpcMessage &msg)
{
//	shvLogFuncFrame() << msg.toCpon();
	if(msg.isRequest()) {
		RpcRequest rq(msg);
		qfMessage() << "RPC request received:" << rq.toPrettyString();
		m_rootNode->handleRpcRequest(rq);
	}
	else if(msg.isResponse()) {
		RpcResponse rp(msg);
		qfMessage() << "RPC response received:" << rp.toPrettyString();
	}
	else if(msg.isSignal()) {
		RpcSignal nt(msg);
		qfMessage() << "RPC signal received:" << nt.toPrettyString();
	}
}

void QxEventService::sendRpcMessage(const shv::chainpack::RpcMessage &rpc_msg)
{
	if(m_rpcConnection && m_rpcConnection->isBrokerConnected()) {
		m_rpcConnection->sendRpcMessage(rpc_msg);
	}
}

void QxEventService::subscribeChanges()
{
	Q_ASSERT(m_rpcConnection);
	QString shv_path = "test";
	QString signal_name = shv::chainpack::Rpc::SIG_VAL_CHANGED;
	auto *rpc_call = RpcCall::createSubscriptionRequest(m_rpcConnection, shv_path, signal_name);
	connect(rpc_call, &RpcCall::maybeResult, this, [this, shv_path, signal_name](const ::shv::chainpack::RpcValue &result, const shv::chainpack::RpcError &error) {
		if(error.isValid()) {
			qfError() << "Signal:" << signal_name << "on SHV path:" << shv_path << "subscribe error:" << error.toString();
		}
		else {
			qfMessage() << "Signal:" << signal_name << "on SHV path:" << shv_path << "subscribed successfully" << result.toCpon();
			// generate data change without ret value check
			m_rpcConnection->callShvMethod((shv_path + "/someInt").toStdString(), "set", 123);
			QTimer::singleShot(500, this, [this, shv_path]() {
				m_rpcConnection->callShvMethod((shv_path + "/someInt").toStdString(), "set", 321);
			});
		}
	});
	rpc_call->start();
}

void QxEventService::testRpcCall() const
{
	Q_ASSERT(m_rpcConnection);
	auto *rpc_call = RpcCall::create(m_rpcConnection)
			->setShvPath("test")
			->setMethod("ls");
//			->setTimeout(5000);
	connect(rpc_call, &RpcCall::maybeResult, [](const ::shv::chainpack::RpcValue &result, const shv::chainpack::RpcError &error) {
		if(error.isValid())
			qfError() << "RPC call error:" << error.toString();
		else
			qfInfo() << "Got RPC response, result:" << result.toCpon();
	});
	rpc_call->start();
}

} // namespace Event::services::qx
