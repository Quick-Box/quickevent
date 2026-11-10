#pragma once

#include "../service.h"

namespace shv::iotqt::node { class ShvRootNode; }
namespace shv::iotqt::rpc { class DeviceConnection; }
namespace shv::chainpack { class RpcMessage; class RpcError; }

namespace Event::services::qx {

class QxEventServiceSettings : public ServiceSettings
{
	using Super = ServiceSettings;

	QF_VARIANTMAP_FIELD2(QString, s, setS, hvBrokerUrl, "tcp://localhost?user=qe&password=test")
	QF_VARIANTMAP_FIELD2(bool, is, set, LocalBroker, false)
	// QF_VARIANTMAP_FIELD2(bool, is, set, ExportDatabase, false)
public:
	QxEventServiceSettings(const QVariantMap &o = QVariantMap()) : Super(o) {}
};

class QxEventService : public Service
{
	Q_OBJECT

	using Super = Service;
public:
	QxEventService(QObject *parent);

	static QString serviceId();
	QString serviceDisplayName() const override;
	QString serviceDescription() const override;

	void run() override;
	void stop() override;
	QxEventServiceSettings settings() const {return QxEventServiceSettings(m_settings);}

	QString apiToken() const;
	static int currentConnectionId();

	static QString eventctlShvPath(bool is_local_connection);

	int eventId() const;
	shv::iotqt::rpc::DeviceConnection* rpcConnection() const { return m_rpcConnection; }
protected:
	void loadSettings() override;
	qf::gui::framework::DialogWidget *createDetailWidget() override;
private: // shv
	void onBrokerConnectedChanged(bool is_connected);
	void onRpcMessageReceived(const shv::chainpack::RpcMessage &msg);
	void sendRpcMessage(const shv::chainpack::RpcMessage &rpc_msg);
	void onBrokerSocketError(const QString &err);
	void onBrokerLoginError(const shv::chainpack::RpcError &err);

	void subscribeChanges();
private: // shv
	shv::iotqt::rpc::DeviceConnection *m_rpcConnection = nullptr;
	shv::iotqt::node::ShvRootNode *m_rootNode;
private:
	int m_eventId = 0;
};

}
