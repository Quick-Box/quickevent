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

namespace {
constexpr auto SIG_EVENT_DB_MODE_CHANGED = "eventDbModeChng";
}

//===============================================
// QxClientService
//===============================================
QxEventService::QxEventService(QObject *parent)
	: Super(QxEventService::serviceId(), parent)
	, m_rootNode(new shv::iotqt::node::ShvRootNode(this))
{
	new DotAppNode(m_rootNode);
	m_sqlApiNode = new SqlApiNode(m_rootNode);

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

	auto ss = qxSettings();

	delete m_rpcConnection;
	m_rpcConnection = nullptr;
	resetEventState();

	auto api_token = apiToken();
	if (api_token.isEmpty()) {
		setStatus(Status::Stopped);
		setStatusMessage(tr("API token is not set."), NecroLogLevel::Error);
		return;
	}
	setStatusMessage(tr("Connecting to broker"));

	m_rpcConnection = new DeviceConnection("QuickEvent", this);
	m_rpcConnection->setShvApiVersion(shv::chainpack::IRpcConnection::ShvApiVersion::V3);
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
	resetEventState();
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
	auto ss = qxSettings();
	if (ss.shvBrokerUrl().isEmpty()) {
		ss.setShvBrokerUrl("tcp://localhost?user=test&password=test");
	}
	m_settings = ss;
}

void QxEventService::postponeRecChngSignals()
{
	m_sqlApiNode->postponeRecChngSignals();
}

void QxEventService::releaseRecChngSignals()
{
	m_sqlApiNode->releaseRecChngSignals();
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

QString QxEventService::eventApiShvPath(bool is_local_connection, int event_id)
{
	return QxEventService::eventctlShvPath(is_local_connection) + "/" + QString::number(event_id);
}

QString QxEventService::eventApiShvPath() const
{
	return QxEventService::eventApiShvPath(qxSettings().isLocalBroker(), m_eventId);
}

void QxEventService::onBrokerConnectedChanged(bool is_connected)
{
	if(is_connected) {
		auto ss = qxSettings();
		auto *event_plugin = getPlugin<EventPlugin>();
		const auto &app_config = event_plugin->appDbConfig();
		auto api_token = app_config.qxConfig().apiToken.toStdString();
		auto *rpc_call = shv::iotqt::rpc::RpcCall::create(m_rpcConnection)
				->setShvPath(eventctlShvPath(ss.isLocalBroker()))
				->setMethod("openEventApiKey")
				->setParams(RpcValue::Map { {"api_token", RpcValue(api_token)} });
		connect(rpc_call, &shv::iotqt::rpc::RpcCall::maybeResult, this, [this](const ::shv::chainpack::RpcValue &result, const shv::chainpack::RpcError &error) {
			if (error.isValid()) {
				resetEventState();
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
						->setShvPath(eventApiShvPath())
						->setMethod("eventDbMode");
				connect(rpc_call2, &shv::iotqt::rpc::RpcCall::maybeResult, this, [this](const ::shv::chainpack::RpcValue &result, const shv::chainpack::RpcError &error) {
					if (error.isValid()) {
						setStatusMessage(tr("Event ID: %1, event DB mode error: %2").arg(m_eventId).arg(error.message()), NecroLogLevel::Warning);
					}
					else {
						onEventDbModeChanged(result);
					}
				});
				rpc_call2->start();
			}
		});
		rpc_call->start();
	} else {
		resetEventState();
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
		if (nt.signal() == SIG_EVENT_DB_MODE_CHANGED
				&& nt.shvPath().asString() == eventApiShvPath().toStdString()) {
			onEventDbModeChanged(nt.params());
		}
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
	auto shv_path = eventApiShvPath();
	QString signal_name = SIG_EVENT_DB_MODE_CHANGED;
	auto *rpc_call = RpcCall::createSubscriptionRequestV3(m_rpcConnection, shv_path, signal_name, {});
	connect(rpc_call, &RpcCall::maybeResult, this, [this, shv_path, signal_name](const ::shv::chainpack::RpcValue &result, const shv::chainpack::RpcError &error) {
		Q_UNUSED(result)
		if(error.isValid()) {
			qfWarning() << "Signal:" << signal_name << "on SHV path:" << shv_path << "subscribe error:" << error.toString();
			setStatusMessage(tr("Event ID: %1, subscribe %2 error: %3").arg(m_eventId).arg(signal_name, QString::fromStdString(error.message())), NecroLogLevel::Warning);
		}
		else {
			qfMessage() << "Signal:" << signal_name << "on SHV path:" << shv_path << "subscribed successfully";
		}
	});
	rpc_call->start();
}

void QxEventService::onEventDbModeChanged(const shv::chainpack::RpcValue &params)
{
	QString db_mode;
	if (params.isMap()) {
		const auto &m = params.asMap();
		if (m.hasKey("event_id") && m.value("event_id").toInt() != m_eventId) {
			return;
		}
		db_mode = QString::fromStdString(m.value("db_mode").to<std::string>());
	}
	else {
		db_mode = QString::fromStdString(params.to<std::string>());
	}
	setEventDbMode(db_mode);
}

void QxEventService::setEventDbMode(const QString &db_mode)
{
	if (m_eventDbMode != db_mode) {
		m_eventDbMode = db_mode;
		emit eventDbModeChanged(m_eventDbMode);
	}
	if (m_eventId > 0) {
		// show DB mode in the services overview, but do not hide pending warnings / errors
		auto level = statusMessageLevel();
		if (level != NecroLogLevel::Warning && level != NecroLogLevel::Error && level != NecroLogLevel::Fatal) {
			setStatusMessage(tr("Event ID: %1 %2").arg(m_eventId).arg(m_eventDbMode).trimmed());
		}
	}
}

void QxEventService::resetEventState()
{
	m_eventId = 0;
	setEventDbMode({});
}

} // namespace Event::services::qx
