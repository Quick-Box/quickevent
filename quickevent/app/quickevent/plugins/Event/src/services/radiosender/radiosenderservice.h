#pragma once

#include "../service.h"

#include <QStringList>

class QTcpServer;
class QTcpSocket;

namespace Event::services {

class RadioSenderService : public Service
{
	Q_OBJECT
	using Super = Service;
public:
	explicit RadioSenderService(QObject *parent);

	void run() override;
	void stop() override;

	static QString serviceName();
	QString serviceDisplayName() const override;
	QString serviceDescription() const override;
	const QStringList &receivedLineLog() const { return m_receivedLineLog; }

	Q_SIGNAL void receivedLineLogged();

	void processLine(const QByteArray &line);
private:
	qf::gui::framework::DialogWidget *createDetailWidget() override;
	void startServer();
	void onNewConnection();
	void onReadyRead();

	QTcpServer *m_server = nullptr;
	QHash<QTcpSocket*, QByteArray> m_clients;
	QStringList m_receivedLineLog;
};

} // namespace Event::services
