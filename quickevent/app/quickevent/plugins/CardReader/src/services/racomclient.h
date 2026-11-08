#pragma once

#include <plugins/Event/src/services/service.h>


class QUdpSocket;
class QTcpServer;
class QTcpSocket;
class QTimer;

namespace siut { class DeviceDriver; }

namespace CardReader {
namespace services {

class RacomClientSettings : public Event::services::ServiceSettings
{
	using Super = Event::services::ServiceSettings;

	QF_VARIANTMAP_FIELD(bool, is, set, ListenRawData)
	QF_VARIANTMAP_FIELD(int, r, setR, awDataListenPort)
	QF_VARIANTMAP_FIELD(bool, is, set, ListenSirxdData)
	QF_VARIANTMAP_FIELD(int, s, setS, irxdDataListenPort)
	QF_VARIANTMAP_FIELD(bool, is, set, ReadSplitFile)
	QF_VARIANTMAP_FIELD(QString, s, setS, plitFileName)
	QF_VARIANTMAP_FIELD2(int, s, setS, plitFileInterval,5)
	QF_VARIANTMAP_FIELD2(int, s, setS, plitFileFinishCode,2)

public:
	RacomClientSettings(const QVariantMap &o = QVariantMap()) : Super(o) {}
};

class RacomClientSirxdConnection : public QObject
{
	Q_OBJECT

	using Super = QObject;
public:
	RacomClientSirxdConnection(QTcpSocket *socket, QObject *parent);
private:
	void onReadyRead();
private:
	QTcpSocket *m_socket = nullptr;
	QByteArray m_data;
};

class RacomReadSplitFile : public QObject
{
	Q_OBJECT

	using Super = QObject;
public:
	RacomReadSplitFile(QString fileName, int interval, int finishCode, QObject *parent);
	~RacomReadSplitFile();
	void readAndProcessFile();
	void run();
private:
	QString m_fileName;
	int m_interval = 5;
	int m_finishCode = 2;
	int m_lastRowCount = 0;
	QTimer *m_readTimer = nullptr;
};

class RacomClient : public Event::services::Service
{
	Q_OBJECT

	using Super = Service;
public:
	RacomClient(QObject *parent);

	void run() override;
	void stop() override;
	RacomClientSettings settings() const {return RacomClientSettings(m_settings);}

	static QString serviceName();
	QString serviceDescription() const override;
private:
	void onDbEventNotify(const QString &domain, int connection_id, const QVariant &data);
	void onRawSIDataUdpSocketReadyRead();
	qf::gui::framework::DialogWidget *createDetailWidget() override;
	QUdpSocket *rawDataUdpSocket();
	void init();
private:
	QUdpSocket *m_rawSIDataUdpSocket = nullptr;
	QTcpServer *m_sirxdDataServer = nullptr;
	siut::DeviceDriver *m_siDriver = nullptr;
	RacomClientSirxdConnection* m_racomSirxdConnection = nullptr;
	RacomReadSplitFile* m_racomSplitFile = nullptr;
};

}}

