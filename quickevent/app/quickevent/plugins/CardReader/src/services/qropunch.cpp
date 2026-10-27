#include "qropunch.h"
#include "qropunchwidget.h"

#include "../cardreaderplugin.h"

#include <quickevent/core/si/checkedcard.h>
#include <quickevent/core/codedef.h>

#include <qf/gui/framework/mainwindow.h>

#include <qf/core/log.h>

#include <QTcpSocket>
#include <QJsonParseError>
#include <QJsonDocument>

#include <memory>
#include <regex>

using qf::gui::framework::getPlugin;


namespace CardReader::services {

QrOPunch::QrOPunch(QObject *parent)
	: Super(QrOPunch::serviceName(), parent)
{
	auto onSettingsChanged = [this] {
		if (m_tcpServer) {
			init();
		}
	};
	connect(this, &QrOPunch::settingsChanged, this, onSettingsChanged, Qt::QueuedConnection);
}

void QrOPunch::run()
{
	init();
	Super::run();
}

void QrOPunch::stop()
{
	Super::stop();
	m_tcpClients.clear();
	delete m_tcpServer, m_tcpServer = nullptr;
}

QString QrOPunch::serviceDescription() const
{
	return tr("Receives punches over TCP from the QR-O-Punch mobile app.\nhttps://github.com/sakhnik/qr-o-punch");
}

QString QrOPunch::serviceName()
{
	return QStringLiteral("QrOPunch");
}

qf::gui::framework::DialogWidget *QrOPunch::createDetailWidget()
{
	auto *w = new QrOPunchWidget();
	return w;
}

void QrOPunch::init()
{
	QrOPunchSettings ss = settings();
	int port = ss.tcpListenPort();

	m_tcpClients.clear();
	
		delete m_tcpServer;
	
	m_tcpServer = new QTcpServer(this);
	connect(m_tcpServer, &QTcpServer::newConnection, this, &QrOPunch::onNewConnection);

	if (!m_tcpServer->listen(QHostAddress::Any, port)) {
		qfError() << "Server could not start listening port " << port << ": " << m_tcpServer->errorString();
	} else {
		qfInfo() << "TCP server started listening port " << port;
	}
	if (ss.isWriteLogFile()) {
		m_logFileFs.close();
		m_logFileFs.open(ss.logFileName().toLocal8Bit(), std::ios_base::app);
		if (!m_logFileFs) {
			qfError() << "Failed to open file for logging: " << ss.logFileName();
		} else {
			qfInfo() << "Opened file for logging: " << ss.logFileName();
		}
	} else {
		m_logFileFs.close();
	}
}

void QrOPunch::onNewConnection()
{
	while (m_tcpServer->hasPendingConnections())
	{
		QTcpSocket *socket = m_tcpServer->nextPendingConnection();

		connect(socket, &QTcpSocket::readyRead, this, &QrOPunch::onReadyRead);
		connect(socket, &QTcpSocket::disconnected, this, [this] {
			auto *socket = qobject_cast<QTcpSocket*>(sender());
			m_tcpClients.remove(socket->socketDescriptor());
			socket->deleteLater();
		});
		connect(socket, &QTcpSocket::errorOccurred, this, [socket] {
			qfError() << "Socket error: " << socket->errorString();
			socket->close();
		});
		m_tcpClients.insert(socket->socketDescriptor(), {});
	}
}

void QrOPunch::onReadyRead()
{
	auto *socket = qobject_cast<QTcpSocket*>(sender());
	auto sendError = [socket](const char *error) {
		QTcpSocket::connect(socket, &QTcpSocket::bytesWritten, [socket](qint64) {
			if (!socket->bytesToWrite())
				socket->close();
		});
		socket->write("HTTP/1.1 ");
		socket->write(error);
		socket->write("\r\n\r\n");
		socket->flush();
	};
	auto it = m_tcpClients.find(socket->socketDescriptor());
	assert(it != m_tcpClients.end() && "The socket should be associated with a client");
	QByteArray &buffer = it.value();
	while (socket->bytesAvailable() > 0)
	{
		buffer.append(socket->readAll());

		// Parse the first request line
		int eol = buffer.indexOf("\r\n");
		if (-1 == eol) {
			continue;
		}
		const static std::regex rex("^([A-Z]+) ([^:]+://[^/]*|)([^ ]+)");
		std::cmatch match;
		if (!std::regex_search(buffer.constData(), buffer.constData() + eol, match, rex)) {
			sendError("405 Method Not Allowed");
			continue;
		}
		if (match[1].str() != "POST") {
			sendError("405 Method Not Allowed");
			continue;
		}
		if (match[3].str() != "/card") {
			sendError("404 Not Found");
			continue;
		}

		// Figure out the whole request length and wait for it
		const std::string SIZE = "Content-Length: ";
		int lbegin = buffer.indexOf(SIZE.data());
		if (-1 == lbegin) {
			if (-1 != buffer.indexOf("\r\n\r\n")) {
				sendError("405 Method Not Allowed");
			}
			continue;
		}
		lbegin += SIZE.size();
		int lend = buffer.indexOf("\r", lbegin);
		if (-1 == lend) {
			continue;
		}
		std::string clength(buffer.constData() + lbegin, buffer.constData() + lend);
		int len = std::stoi(clength);
		int dataIdx = buffer.indexOf("\r\n\r\n");
		if (-1 == dataIdx) {
			continue;
		}
		dataIdx += 4;
		if (buffer.size() < dataIdx + len) {
			continue;
		}

		// Process POST /card
		QByteArray data{buffer.mid(dataIdx, len)};
		if (m_logFileFs) {
			m_logFileFs << QDateTime::currentDateTime().toString("\ndd.MM.yyyy hh:mm:ss\n").toUtf8().constData();
			m_logFileFs.write(buffer.constData(), dataIdx + len);
			m_logFileFs << std::endl;
		}
		QJsonParseError parseError{};
		auto jsonDoc = QJsonDocument::fromJson(data, &parseError);
		if (jsonDoc.isNull()) {
			QTcpSocket::connect(socket, &QTcpSocket::bytesWritten, [socket](qint64) {
				if (!socket->bytesToWrite())
					socket->close();
			});
			socket->write("HTTP/1.1 400 Bad Request\r\n\r\n");
			socket->write("JSON error at the offset ");
			socket->write(std::to_string(parseError.offset).c_str());
			socket->write(":\n");
			socket->write(parseError.errorString().toUtf8());
			socket->flush();
			break;
		}
		onTcpReadoutReceived(jsonDoc.toVariant());
		QTcpSocket::connect(socket, &QTcpSocket::bytesWritten, [socket](qint64) {
			if (!socket->bytesToWrite())
				socket->close();
		});
		socket->write("HTTP/1.1 200 OK\r\n\r\n");
		socket->flush();
        break;
	}
}

void QrOPunch::onTcpReadoutReceived(const QVariant &data)
{
	if (!m_siDriver) {
		m_siDriver = std::make_unique<siut::DeviceDriver>(this);
		connect(m_siDriver.get(), &siut::DeviceDriver::siTaskFinished, getPlugin<CardReaderPlugin>(), &CardReader::CardReaderPlugin::emitSiTaskFinished);
	}
	getPlugin<CardReaderPlugin>()->emitSiTaskFinished(static_cast<int>(siut::SiTask::Type::CardRead), data);
	siut::SICard card = siut::SICard::fromVariantMap(data.toMap());
	for (int i = 0, n = card.punchCount(); i < n; ++i) {
		getPlugin<CardReaderPlugin>()->emitSiTaskFinished(static_cast<int>(siut::SiTask::Type::Punch), card.punchAt(i).toVariantMap());
	}
}

}
