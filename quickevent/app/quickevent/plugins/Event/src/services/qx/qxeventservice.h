#pragma once

#include "../service.h"

class QNetworkAccessManager;
class QNetworkReply;
class QUrlQuery;
class QTimer;

namespace shv::iotqt::node { class ShvNodeTree; }
namespace shv::iotqt::rpc { class ClientConnection; }
namespace shv::chainpack { class RpcMessage; class RpcError; }

namespace Event::services::qx {

class QxEventServiceSettings : public ServiceSettings
{
	using Super = ServiceSettings;

	QF_VARIANTMAP_FIELD2(QString, e, setE, xchangeServerUrl, "http://localhost:8000")
public:
	QxEventServiceSettings(const QVariantMap &o = QVariantMap()) : Super(o) {}
};

class EventInfo : public QVariantMap
{
private:
	typedef QVariantMap Super;

	QF_VARIANTMAP_FIELD(int, i, set_i, d)
	QF_VARIANTMAP_FIELD(int, s, set_s, tage)
	QF_VARIANTMAP_FIELD(int, s, set_s, tage_count)
	QF_VARIANTMAP_FIELD(QString, n, set_n, ame)
	QF_VARIANTMAP_FIELD(QString, p, set_p, lace)
	QF_VARIANTMAP_FIELD(QString, s, set_s, tart_time)
	QF_VARIANTMAP_FIELD(QVariantList, c, set_c, lasses)
public:
	EventInfo(const QVariantMap &data = QVariantMap()) : QVariantMap(data) {}
};

class QxEventService : public Service
{
	Q_OBJECT

	using Super = Service;
public:
	static constexpr auto QX_API_TOKEN = "qx-api-token";
public:
	QxEventService(QObject *parent);

	static QString serviceId();
	QString serviceDisplayName() const override;

	void run() override;
	void stop() override;
	QxEventServiceSettings settings() const {return QxEventServiceSettings(m_settings);}

	void onDbEventNotify(const QString &domain, int connection_id, const QVariant &data);
	QNetworkAccessManager* networkManager();

	QNetworkReply* getRemoteEventInfo(const QString &qxhttp_host, const QString &api_token);
	QNetworkReply* postEventInfo(const QString &qxhttp_host, const QString &api_token);

	void postStartListIofXml3(QObject *context, std::function<void (QString)> call_back = nullptr);
	void postRuns(QObject *context, std::function<void (QString)> call_back = nullptr);
	void getHttpJson(const QString &path, const QUrlQuery &query, QObject *context, const std::function<void (QVariant json, QString error)> &call_back = nullptr);

	QNetworkReply* getQxChangesReply(int from_id);

	QByteArray apiToken() const;
	static int currentConnectionId();
	QUrl exchangeServerUrl() const;

	int eventId() const;
private: // shv
	void onBrokerConnectedChanged(bool is_connected);
	void onRpcMessageReceived(const shv::chainpack::RpcMessage &msg);
	void onBrokerSocketError(const QString &err);
	void onBrokerLoginError(const shv::chainpack::RpcError &err);

	void subscribeChanges();
	void testRpcCall() const;
private:
	void loadSettings() override;
	qf::gui::framework::DialogWidget *createDetailWidget() override;
	void postFileCompressed(std::optional<QString> path, std::optional<QString> name, QByteArray data, QObject *context, std::function<void(QString error)> call_back = nullptr);
	enum class SpecFile {StartListIofXml3, RunsCsvJson};
	void uploadSpecFile(SpecFile file, QByteArray data, QObject *context, const std::function<void(QString error)> &call_back = nullptr);
	QByteArray zlibCompress(QByteArray data);

	void httpPostJson(const QString &path, const QString &query, QVariantMap json, QObject *context = nullptr, const std::function<void (QString)> &call_back = nullptr);

	void connectToSSE(int event_id);
	void disconnectSSE();

	void pollQxChanges();

	EventInfo eventInfo() const;
private: // shv
	shv::iotqt::rpc::ClientConnection *m_rpcConnection = nullptr;
	shv::iotqt::node::ShvNodeTree *m_shvTree = nullptr;

private:
	QNetworkAccessManager *m_networkManager = nullptr;
	QNetworkReply *m_replySSE = nullptr;
	int m_eventId = 0;
	QTimer *m_pollChangesTimer = nullptr;
};

}
