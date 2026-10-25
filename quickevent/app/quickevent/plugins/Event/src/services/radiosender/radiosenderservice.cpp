#include "radiosenderservice.h"
#include "radiosenderservicewidget.h"
#include "radiosenderconfig.h"

#include "../../eventplugin.h"

#include <plugins/Runs/src/runsplugin.h>

#include <quickevent/core/si/checkedcard.h>

#include <qf/core/log.h>
#include <qf/core/sql/query.h>
#include <qf/core/sql/qxsql.h>
#include <qf/gui/framework/application.h>
#include <qf/gui/framework/mainwindow.h>

#include <QTcpServer>
#include <QTcpSocket>

using qf::gui::framework::getPlugin;

namespace Event::services {

RadioSenderService::RadioSenderService(QObject *parent)
	: Super(serviceName(), parent)
{
	m_server = new QTcpServer(this);
	connect(m_server, &QTcpServer::newConnection, this, &RadioSenderService::onNewConnection);
}

QString RadioSenderService::serviceName()
{
	return QStringLiteral("RadioSender");
}

QString RadioSenderService::serviceDisplayName() const
{
	return tr("Radio Sender");
}

void RadioSenderService::run()
{
	startServer();
	if (m_server->isListening())
		Super::run();
}

void RadioSenderService::stop()
{
	m_server->close();
	// disconnectFromHost() may emit disconnected() synchronously, which removes the socket from m_clients
	const auto clients = std::exchange(m_clients, {});
	for (auto it = clients.keyBegin(); it != clients.keyEnd(); ++it) {
		QTcpSocket *socket = *it;
		socket->disconnectFromHost();
		socket->deleteLater();
	}
	Super::stop();
}

qf::gui::framework::DialogWidget *RadioSenderService::createDetailWidget()
{
	return new RadioSenderServiceWidget(this);
}

void RadioSenderService::startServer()
{
	const auto config = getPlugin<EventPlugin>()->appDbConfig().radioSenderConfig();
	const QHostAddress address(config.listenAddress);
	if (address.isNull() || config.port <= 0 || config.port > 65535) {
		setStatusMessage(tr("Invalid listen address or port"));
		return;
	}
	if (!m_server->listen(address, static_cast<quint16>(config.port))) {
		setStatusMessage(tr("Cannot listen on %1:%2: %3")
			.arg(config.listenAddress).arg(config.port).arg(m_server->errorString()));
		return;
	}
	setStatusMessage(tr("Listening on %1:%2").arg(config.listenAddress).arg(config.port));
}

void RadioSenderService::onNewConnection()
{
	while (m_server->hasPendingConnections()) {
		QTcpSocket *socket = m_server->nextPendingConnection();
		qfInfo() << "New connection from" << socket->peerAddress().toString();
		m_clients.insert(socket, {});
		connect(socket, &QTcpSocket::readyRead, this, &RadioSenderService::onReadyRead);
		connect(socket, &QTcpSocket::disconnected, this, [this, socket] {
			m_clients.remove(socket);
			socket->deleteLater();
		});
		connect(socket, &QTcpSocket::errorOccurred, this, [socket](QAbstractSocket::SocketError error) {
			if (error == QAbstractSocket::SocketError::RemoteHostClosedError) {
				qfInfo() << "Remote host" << socket->peerAddress().toString() << "closed connection";
			} else {
				qfWarning() << "RadioSender socket error:" << socket->errorString();
			}
		});
	}
	setStatusMessage(tr("Listening, %1 sender(s) connected").arg(m_clients.size()));
}

void RadioSenderService::onReadyRead()
{
	auto *socket = qobject_cast<QTcpSocket*>(sender());
	if (!socket || !m_clients.contains(socket))
		return;
	QByteArray &buffer = m_clients[socket];
	buffer += socket->readAll();
	while (true) {
		const qsizetype eol = buffer.indexOf('\n');
		if (eol < 0)
			break;
		const QByteArray line = buffer.left(eol).trimmed();
		buffer.remove(0, eol + 1);
		if (!line.isEmpty())
			processLine(line);
	}
}

void RadioSenderService::processLine(const QByteArray &line)
{
    // {Control};{Bib};{Time:HH:mm:ss.fff};{Status};{Cancellation}
    enum { ColControl = 0, ColBib, ColTime, ColStatus, ColCancellation };
	const QList<QByteArray> fields = line.split(';');
	bool id_ok = false;
	bool control_ok = false;
	const int start_number = fields.value(ColBib).trimmed().toInt(&id_ok);
	const int control = fields.value(ColControl).trimmed().toInt(&control_ok);
	const QTime time = QTime::fromString(QString::fromLatin1(fields.value(ColTime).trimmed()), QStringLiteral("HH:mm:ss.zzz"));
	bool is_dns = fields.value(ColStatus).trimmed() == "DNS";
	bool is_cancellation = fields.value(ColCancellation).trimmed() == "ANN";
	const auto config = getPlugin<EventPlugin>()->appDbConfig().radioSenderConfig();
	const bool is_valid = (control == config.startControl || control == config.finishControl)
		&& (time.isValid() || is_cancellation || is_dns);
	QString parsed_data;
	if (is_valid) {
		parsed_data = QStringLiteral("%1, startNumber: %2, time=%3")
			.arg(control == config.startControl ? QStringLiteral("STA") : QStringLiteral("FIN"))
			.arg(start_number)
			.arg(time.toString(QStringLiteral("HH:mm:ss.zzz")));
	} else {
		parsed_data = QStringLiteral("invalid message");
	}
	m_receivedLineLog.append(QStringLiteral("%1\n  %2")
		.arg(QString::fromUtf8(line), parsed_data));
	while (m_receivedLineLog.size() > 20) {
		m_receivedLineLog.removeFirst();
	}
	emit receivedLineLogged();
	if (!is_valid) {
		qfWarning() << "RadioSender: invalid message:" << line;
		return;
	}

	auto *event_plugin = getPlugin<EventPlugin>();
	const int stage_id = event_plugin->currentStageId();
	qf::core::sql::Query q;
	q.prepare("SELECT runs.id FROM runs JOIN competitors ON competitors.id=runs.competitorId"
		" WHERE runs.stageId=:stageId"
		" AND runs.isRunning"
		" AND competitors.startNumber=:startNumber");
	q.bindValue(QStringLiteral(":stageId"), stage_id);
	q.bindValue(QStringLiteral(":startNumber"), start_number);
	q.exec(qf::core::Exception::Throw);
	if (!q.next()) {
		qfWarning() << "RadioSender: competitor not found:" << start_number;
		return;
	}
	const int run_id = q.value(0).toInt();
	if (q.next()) {
		qfWarning() << "RadioSender: ambiguous competitor id:" << start_number;
		return;
	}

	QVariantMap record;
	const QString field = control == config.startControl
				? QStringLiteral("startGateTime")
				: QStringLiteral("finishGateTime");
	if (time.isValid()) {
		const QDateTime stage_start = event_plugin->stageStartDateTime(stage_id);
		QDateTime gate_time(stage_start.date(), time);
		record[field] = gate_time;
	}
	if (is_dns) {
		record[QStringLiteral("notstart")] = true;
	}
	if (is_cancellation) {
		record[field] = {};
	}

	qf::gui::framework::Application::instance()->qxSql()->updateRecord( QStringLiteral("runs"), run_id, record, this);
	getPlugin<Runs::RunsPlugin>()->computeStageTime(run_id);
}

} // namespace Event::services
