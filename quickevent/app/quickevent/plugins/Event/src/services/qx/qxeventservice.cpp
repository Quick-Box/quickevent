#include "qxeventservice.h"
#include "qxeventservicewidget.h"
#include "nodes.h"
#include "sqlapinode.h"

#include "../../eventplugin.h"

#include <qf/gui/framework/mainwindow.h>
#include <qf/core/log.h>
#include <qf/core/sql/connection.h>

#include <shv/iotqt/rpc/deviceconnection.h>
#include <shv/iotqt/node/shvnodetree.h>
#include <shv/iotqt/rpc/rpccall.h>

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QSettings>
#include <QStandardPaths>
#include <QTextStream>

using namespace shv::chainpack;
using namespace shv::iotqt::rpc;
using namespace shv::iotqt::node;

using namespace qf::core;
using namespace qf::gui;
using namespace qf::gui::dialogs;
using namespace qf::core::sql;
using qf::gui::framework::getPlugin;
using Event::EventPlugin;

namespace Event::services::qx {

//===============================================
// QxClientService
//===============================================
QxEventService::QxEventService(QObject *parent)
	: Super(QxEventService::serviceId(), parent)
	, m_rootNode(new shv::iotqt::node::ShvRootNode(this))
{
	new DotAppNode(m_rootNode);
	new SqlApiNode(m_rootNode);

	connect(m_rootNode, &shv::iotqt::node::ShvNode::sendRpcMessage, this, &QxEventService::sendRpcMessage);
}

QString QxEventService::serviceDescription() const
{
	return tr("Electronic late entries.\nhttps://qxqx.org");
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
	m_rpcConnection = nullptr;
	m_eventId = 0;

	auto api_token = apiToken();
	if (api_token.isEmpty()) {
		setStatus(Status::Stopped);
		setStatusMessage(tr("API token is not set."), NecroLogLevel::Error);
		return;
	}
	setStatusMessage(tr("Connecting to broker"));

	m_rpcConnection = new DeviceConnection("QuickEvent", this);
	m_rpcConnection->setConnectionString(ss.shvBrokerUrl());
	{
		RpcValue::Map opts;
		RpcValue::Map device;
		device["deviceId"] = api_token.toStdString();
		opts["device"] = device;
		m_rpcConnection->setConnectionOptions(opts);
	}

	connect(m_rpcConnection, &ClientConnection::brokerConnectedChanged, this, &QxEventService::onBrokerConnectedChanged);
	connect(m_rpcConnection, &ClientConnection::socketError, this, &QxEventService::onBrokerSocketError);
	connect(m_rpcConnection, &ClientConnection::brokerLoginError, this, &QxEventService::onBrokerLoginError);
	connect(m_rpcConnection, &ClientConnection::rpcMessageReceived, this, &QxEventService::onRpcMessageReceived);

	// connect(::qx::SqlApi::instance(), &::qx::SqlApi::recchng, this, &QxEventService::sendRecchgShvSignal);

	m_rpcConnection->open();
}

void QxEventService::stop()
{
	delete m_rpcConnection;
	m_rpcConnection = nullptr;
	m_eventId = 0;
	Super::stop();
	setStatusMessage({});
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

int QxEventService::eventId() const
{
	// if (m_eventId == 0) {
	// 	throw qf::core::Exception(tr("Event ID is not loaded, service is not probably running."));
	// }
	return m_eventId;
}

QString QxEventService::apiToken() const
{
	auto *event_plugin = getPlugin<EventPlugin>();
	return event_plugin->appDbConfig().qxConfig().apiToken;
}

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

QString QxEventService::eventctlShvPath(bool is_local_connection)
{
	QString ret = "qx/qxeventd/eventctl";
	if (is_local_connection) {
		ret = "test/" + ret;
	}
	return ret;
}

void QxEventService::onBrokerConnectedChanged(bool is_connected)
{
	if(is_connected) {
		auto ss = settings();
		auto *event_plugin = getPlugin<EventPlugin>();
		const auto &app_config = event_plugin->appDbConfig();
		auto api_token = app_config.qxConfig().apiToken.toStdString();
		auto *rpc_call = shv::iotqt::rpc::RpcCall::create(m_rpcConnection)
				->setShvPath(eventctlShvPath(ss.isLocalBroker()))
				->setMethod("openEventApiKey")
				->setParams(RpcValue::Map { {"api_token", RpcValue(api_token)} });
		connect(rpc_call, &shv::iotqt::rpc::RpcCall::maybeResult, this, [this](const ::shv::chainpack::RpcValue &result, const shv::chainpack::RpcError &error) {
			if (error.isValid()) {
				setStatus(Status::Stopped);
				setStatusMessage(tr("Open event error: %1").arg(error.toString()), NecroLogLevel::Error);
			}
			else {
				const auto &info = result.asMap();
				m_eventId = info.value("event_id").toInt();
				setStatus(Status::Running);
				setStatusMessage(tr("Event ID: %1").arg(m_eventId));
				subscribeChanges();
				auto *rpc_call2 = shv::iotqt::rpc::RpcCall::create(m_rpcConnection)
						->setShvPath(eventctlShvPath(settings().isLocalBroker()))
						->setMethod("eventData")
						->setParams(RpcValue::Map{{"event_id", m_eventId}});
				connect(rpc_call2, &shv::iotqt::rpc::RpcCall::maybeResult, this, [this](const ::shv::chainpack::RpcValue &result, const shv::chainpack::RpcError &error) {
					if (error.isValid()) {
						setStatusMessage(tr("Event ID: %1, event data error: %2").arg(m_eventId).arg(error.message()), NecroLogLevel::Warning);
					}
					else {
						setStatusMessage(tr("Event ID: %1 %2").arg(m_eventId).arg(result.asMap().value("db_mode").to<QString>()));
					}
				});
				rpc_call2->start();
			}
		});
		rpc_call->start();
	} else {
		setStatus(Status::Stopped);
	}

}

void QxEventService::onBrokerSocketError(const QString &err)
{
	qfWarning() << "onBrokerSocketError:" << err;
	setStatusMessage(tr("Broker socket error: %1").arg(err), NecroLogLevel::Error);
}

void QxEventService::onBrokerLoginError(const shv::chainpack::RpcError &err)
{
	qfWarning() << "onBrokerLoginError:" << err.toString();
	setStatusMessage(tr("Broker login error: %1").arg(err.toString()), NecroLogLevel::Error);
}

void QxEventService::onRpcMessageReceived(const shv::chainpack::RpcMessage &msg)
{
//	shvLogFuncFrame() << msg.toCpon();
	if(msg.isRequest()) {
		RpcRequest rq(msg);
		if (rq.shvPath().asString().starts_with(".broker/")) {
			// ignore broker discovery messages
			return;
		}
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
	// Q_ASSERT(m_rpcConnection);
	// QString shv_path = "test";
	// QString signal_name = shv::chainpack::Rpc::SIG_VAL_CHANGED;
	// auto *rpc_call = RpcCall::createSubscriptionRequest(m_rpcConnection, shv_path, signal_name);
	// connect(rpc_call, &RpcCall::maybeResult, this, [shv_path, signal_name](const ::shv::chainpack::RpcValue &result, const shv::chainpack::RpcError &error) {
	// 	if(error.isValid()) {
	// 		qfError() << "Signal:" << signal_name << "on SHV path:" << shv_path << "subscribe error:" << error.toString();
	// 	}
	// 	else {
	// 		qfMessage() << "Signal:" << signal_name << "on SHV path:" << shv_path << "subscribed successfully" << result.toCpon();
	// 	}
	// });
	// rpc_call->start();
}

// void QxEventService::sendRecchgShvSignal(const qf::core::sql::QxRecChng &chng)
// {
// 	if (isRunning()) {
// 		m_rpcConnection->sendShvSignal("sql", "recchng", ::qx::qxRecChngToRpcValue(chng));
// 	}
// }

} // namespace Event::services::qx
