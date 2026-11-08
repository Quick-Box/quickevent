#include "racomclient.h"
#include "racomclientwidget.h"

#include "../cardreaderplugin.h"

#include <quickevent/core/si/checkedcard.h>
#include <quickevent/core/codedef.h>

//#include <siut/simessagedata.h>
#include <siut/sidevicedriver.h>

#include <qf/gui/framework/mainwindow.h>
#include <qf/gui/dialogs/dialog.h>

#include <qf/core/log.h>
#include <qf/core/assert.h>
#include <qf/core/sql/querybuilder.h>
#include <qf/core/sql/query.h>
#include <qf/core/sql/connection.h>
#include <plugins/Event/src/eventplugin.h>

#include <QException>
#include <QTimer>
#include <QFile>
#include <QSettings>
#include <QStandardPaths>
#include <QTcpServer>
#include <QUdpSocket>
#include <QTcpSocket>
#if (QT_VERSION >= QT_VERSION_CHECK(5, 8, 0))
#include <QNetworkDatagram>
#endif

//namespace qfm = qf::gui::model;
using qf::gui::framework::getPlugin;
using Event::EventPlugin;
using CardReader::CardReaderPlugin;


namespace CardReader::services {

RacomClientSirxdConnection::RacomClientSirxdConnection(QTcpSocket *socket, QObject *parent)
	: Super(parent)
	, m_socket(socket)
{
	m_socket->setParent(this);
	connect(socket, &QAbstractSocket::disconnected, this, &QObject::deleteLater);
	connect(socket, &QAbstractSocket::readyRead, this, &RacomClientSirxdConnection::onReadyRead);
}

static bool parse_time(const QByteArray &ba, int &secs, int &msecs)
{
	bool ok;
	if(ba.indexOf('.') > 0) {
		double d = ba.toDouble(&ok);
		secs = (int)d;
		msecs = static_cast<int>(d * 1000) % 1000;
	}
	else {
		secs = ba.toInt(&ok);
		if(secs < 0)
			secs = siut::SICard::INVALID_SI_TIME;
		msecs = 0;
	}
	return ok;
}

void RacomClientSirxdConnection::onReadyRead()
{
	m_data += m_socket->readAll();
	while (true) {
		int ix = m_data.indexOf('\n');
		if(ix < 0)
			break;
		QByteArray rec = m_data.mid(0, ix).trimmed();
		m_data = m_data.mid(ix + 1);
		static const QByteArray READOUT("Readout;");
		static const QByteArray SPLIT("Split;");
		if(rec.startsWith(READOUT)) {
			enum {
				ColCardNumber = 1,
				ColCheckTime = 2,
				ColStartTime = 4,
				ColFinishTime = 6,
				ColPunchCount = 8,
			};
			QList<QByteArray> splits = rec.split(';');
			siut::SICard card;
			card.cardNumber = splits.value(ColCardNumber).toInt();
			int secs, msecs;
			parse_time(splits.value(ColCheckTime), secs, msecs);
			card.checkTime = secs;
			parse_time(splits.value(ColStartTime), secs, msecs);
			card.startTime = secs;
			parse_time(splits.value(ColFinishTime), secs, msecs);
			card.finishTime   = secs;
			card.finishTimeMs = msecs;
			int punch_cnt = splits.value(ColPunchCount).toInt();
			siut::SICard::PunchList punches;
			for (int i = 0; i < punch_cnt; ++i) {
				siut::SIPunch punch;
				punch.code = splits.value(ColPunchCount + 1 + (3*i) + 0).toInt();
				parse_time(splits.value(ColPunchCount + 1 + (3*i) + 1), secs, msecs);
				punch.time = secs;
				punch.msec = msecs;
				punches << punch;
			}
			card.punches = punches;
			getPlugin<CardReaderPlugin>()->emitSiTaskFinished((int)siut::SiTask::Type::CardRead, card.toVariantMap());
		}
		else if(rec.startsWith(SPLIT)) {
			enum {
				ColCardNumber = 1,
				ColCode = 2,
				ColTime = 3,
			};
			QList<QByteArray> splits = rec.split(';');
			siut::SIPunch punch;
			punch.cardNumber = splits.value(ColCardNumber).toInt();
			punch.code = splits.value(ColCode).toInt();
			int secs, msecs;
			parse_time(splits.value(ColTime), secs, msecs);
			punch.time = secs;
			punch.msec = msecs;
			getPlugin<CardReaderPlugin>()->emitSiTaskFinished((int)siut::SiTask::Type::Punch, punch.toVariantMap());
		}
		else {
			qfWarning() << "Throwing away unrecognised sirxd message:" << QString::fromUtf8(rec);
		}
	}
}

RacomReadSplitFile::RacomReadSplitFile(QString fileName, int interval, int finishCode, QObject *parent)
	: Super(parent)
	, m_fileName(fileName)
	, m_interval(interval)
	, m_finishCode(finishCode)
{
	m_readTimer = new QTimer(this);
	connect(m_readTimer, &QTimer::timeout, this, &RacomReadSplitFile::readAndProcessFile);
}

RacomReadSplitFile::~RacomReadSplitFile()
{
	m_readTimer->stop();
	QF_SAFE_DELETE(m_readTimer)
}

void RacomReadSplitFile::run()
{
	m_readTimer->start(m_interval*1000);
}

void RacomReadSplitFile::readAndProcessFile()
{
	QStringList text;
	QFile file(m_fileName);
	if(file.open(QIODevice::ReadOnly|QIODevice::Text)) {
		QTextStream stream(&file);
		QString line;
		while (stream.readLineInto(&line)) {
			text << line;
		}
		file.close();
		qfInfo() << "Read rawsplits file done, parsing " << text.size() << " lines";
	}
	else
		qfWarning() << "Read split file failed to open " << m_fileName;

	QList <siut::SIPunch> punches;
	if (!text.empty() && m_lastRowCount < text.size()) {
		// parse punches from lines
		for (int i = m_lastRowCount; i < text.size(); i++) {
			if (text[i].size() < 23)	// is not valid line
				continue;

			siut::SIPunch punch;
			punch.cardNumber = QStringView(text[i]).left(8).toInt();
			int code = QStringView(text[i]).mid(9,4).toInt();
			if (code == m_finishCode)
				code = quickevent::core::CodeDef::FINISH_PUNCH_CODE;
			punch.code = code;

			QTime t = QTime::fromString(text[i].right(10),"hh:mm:ss.z");
			int secs = QTime(0,0).secsTo(t);
			int msecs = t.msec();
			punch.time = secs;
			punch.msec = msecs;
			if (!punches.contains(punch))	// remove duplicate punches
				punches.append(punch);
		}
		m_lastRowCount = text.size();

		// add new punches
		for (auto& punch : punches) {
			getPlugin<CardReaderPlugin>()->emitSiTaskFinished((int)siut::SiTask::Type::Punch, punch.toVariantMap());
		}
	}
}

RacomClient::RacomClient(QObject *parent)
	: Super(RacomClient::serviceName(), parent)
{
	connect(getPlugin<EventPlugin>(), &Event::EventPlugin::dbEventNotify, this, &RacomClient::onDbEventNotify, Qt::QueuedConnection);
	connect(this, &RacomClient::settingsChanged, this, &RacomClient::init, Qt::QueuedConnection);
}

void RacomClient::run()
{
	init();
	if (m_racomSplitFile)
		m_racomSplitFile->run();
	Super::run();
}

void RacomClient::stop()
{
	Super::stop();
	QF_SAFE_DELETE(m_rawSIDataUdpSocket);
	QF_SAFE_DELETE(m_sirxdDataServer)
	QF_SAFE_DELETE(m_racomSirxdConnection)
	QF_SAFE_DELETE(m_racomSplitFile)
}

QString RacomClient::serviceDescription() const
{
	return tr("Connects to Racom radio controls.");
}

QString RacomClient::serviceName()
{
	return QStringLiteral("RacomClient");
}

void RacomClient::onDbEventNotify(const QString &domain, int connection_id, const QVariant &data)
{
	Q_UNUSED(connection_id)
	Q_UNUSED(data)
	if(domain == QLatin1String(Event::EventPlugin::DBEVENT_PUNCH_RECEIVED)) {
		//onRacomDataReceived(data.toMap());
	}
}

void RacomClient::onRawSIDataUdpSocketReadyRead()
{
	if(status() != Status::Running)
		return;
#if (QT_VERSION >= QT_VERSION_CHECK(5, 8, 0))
	QUdpSocket *sock = rawDataUdpSocket();
	QNetworkDatagram dg = sock->receiveDatagram();
	QByteArray data = dg.data();
	qfInfo() << data.toHex();
	// remove possible initial garbage
	int i = 0;
	for(i=0; i<data.size(); i++)
		if(data.at(i) == 0x02)
			break;
	data = data.mid(i);
	// remove possible multiple 0x02
	for(i=1; i<data.size(); i++)
		if(data.at(i) != 0x02)
			break;
	data = data.mid(i-1);
	qfInfo() << "stripped data:" << data.toHex();
	if(!m_siDriver) {
		m_siDriver = new siut::DeviceDriver(this);
		connect(m_siDriver, &siut::DeviceDriver::siTaskFinished, getPlugin<CardReaderPlugin>(), &CardReader::CardReaderPlugin::emitSiTaskFinished);
	}
	m_siDriver->processData(data);
#else
	qfWarning() << "Racom client is not supported with Qt < 5.8";
#endif
}

qf::gui::framework::DialogWidget *RacomClient::createDetailWidget()
{
	auto *w = new RacomClientWidget();
	return w;
}

QUdpSocket *RacomClient::rawDataUdpSocket()
{
	return m_rawSIDataUdpSocket;
}

void RacomClient::init()
{
	QF_SAFE_DELETE(m_rawSIDataUdpSocket)
	QF_SAFE_DELETE(m_sirxdDataServer)
	QF_SAFE_DELETE(m_racomSirxdConnection)
	QF_SAFE_DELETE(m_racomSplitFile)
	RacomClientSettings ss = settings();
	//qfDebug() << ss;

	if(ss.isListenRawData()) {
		m_rawSIDataUdpSocket = new QUdpSocket(this);
		if(m_rawSIDataUdpSocket->bind(ss.rawDataListenPort())) {
			connect(m_rawSIDataUdpSocket, &QUdpSocket::readyRead, this, &RacomClient::onRawSIDataUdpSocketReadyRead);
#if QT_VERSION < QT_VERSION_CHECK(5, 15, 0)
			connect(m_rawSIDataUdpSocket, QOverload<QAbstractSocket::SocketError>::of(&QAbstractSocket::error), [](QAbstractSocket::SocketError) {
				qfError() << "RacomClient: raw SI data UDP socket error";
			});
#else
			connect(m_rawSIDataUdpSocket, &QAbstractSocket::errorOccurred, [](QAbstractSocket::SocketError) {
				qfError() << "RacomClient: raw SI data UDP socket error";
			});
#endif
		}
		else {
			qfError() << "Cannot bind raw SI data UDP socket to port:" << ss.rawDataListenPort();
		}
		qfInfo() << "raw SI data UDP socket listenning on port:" << ss.rawDataListenPort();
	}

	if(ss.isListenSirxdData()) {
		m_sirxdDataServer = new QTcpServer(this);
		if (!m_sirxdDataServer->listen(QHostAddress::Any, ss.sirxdDataListenPort())) {
			qfError() << "RacomClient: Sirxd TCP server cannot listen on port:" << ss.sirxdDataListenPort();
		}
		else {
			connect(m_sirxdDataServer, &QTcpServer::newConnection, this, [this](){
				QTcpSocket *sock = m_sirxdDataServer->nextPendingConnection();
				m_racomSirxdConnection = new RacomClientSirxdConnection(sock, m_sirxdDataServer);
			});
		}
		qfInfo() << "Sirxd TCP server listenning on port:" << ss.sirxdDataListenPort();
	}

	if (ss.isReadSplitFile()) {
		m_racomSplitFile = new RacomReadSplitFile(ss.splitFileName(), ss.splitFileInterval(),ss.splitFileFinishCode(),this);
	}
}

}
