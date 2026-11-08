#pragma once

#include <plugins/Event/src/services/service.h>

#include <siut/sidevicedriver.h>

#include <QTcpServer>
#include <fstream>

class QTcpSocket;

namespace CardReader {
namespace services {

class QrOPunchSettings : public Event::services::ServiceSettings
{
	using Super = Event::services::ServiceSettings;

	QF_VARIANTMAP_FIELD(int, t, setT, cpListenPort)
	QF_VARIANTMAP_FIELD(bool, is, set, WriteLogFile)
	QF_VARIANTMAP_FIELD(QString, l, setL, ogFileName)

public:
	QrOPunchSettings(const QVariantMap &o = QVariantMap()) : Super(o) {}
};

class QrOPunch : public Event::services::Service
{
	Q_OBJECT

	using Super = Service;
public:
	QrOPunch(QObject *parent);

	void run() override;
	void stop() override;
	QrOPunchSettings settings() const {return QrOPunchSettings(m_settings);}

	static QString serviceName();
	QString serviceDescription() const override;
private:
	qf::gui::framework::DialogWidget *createDetailWidget() override;
	void init();
	void onTcpReadoutReceived(const QVariant &data);
	void onNewConnection();
	void onReadyRead();
private:
	QTcpServer *m_tcpServer = nullptr;
	QHash<qintptr, QByteArray> m_tcpClients;
	std::unique_ptr<siut::DeviceDriver> m_siDriver;
	std::ofstream m_logFileFs;
};

}}
