
//
// Author: Frantisek Vacek <fanda.vacek@volny.cz>, (C) 2011
//
// Copyright: See COPYING file that comes with this distribution
//

#include "sidevicedriver.h"
#include "crc529.h"

//#include <siut/simessage.h>

#include <qf/core/log.h>

#include <QTimer>
#include <QSettings>
#include <QSerialPortInfo>

#define logCardRead() qfCInfo("CardRead")

namespace siut {

//=================================================
//             DeviceDriver
//=================================================
DeviceDriver::DeviceDriver(QObject *parent)
	: Super(parent)
{
	//NecroLog::checkLogLevelMetaTypeRegistered();
}

DeviceDriver::~DeviceDriver()
= default;

namespace {
/*
	int byte_at(const QByteArray &ba, int ix)
	{
		int ret = -1;
		if(ix < ba.count()) ret = (unsigned char)ba.at(ix);
		return ret;
	}
*/
	void set_byte_at(QByteArray &ba, int ix, char b)
	{
		if(ix == ba.length())
			ba.append(b);
		else if(ix < ba.length())
			ba[ix] = b;
		else
			qfError() << "ByteArray index out of range:" << ix << "size:" << ba.length();
	}
}

void DeviceDriver::processSIMessageData(const SIMessageData &data)
{
	qfLogFuncFrame();
	qfDebug() << data.toString();
	if(m_currentTask) {
		m_currentTask->onSiMessageReceived(data);
		return;
	}
	SIMessageData::Command cmd = data.command();
	switch(cmd) {
	case SIMessageData::Command::SICardRemoved: {
		logCardRead() << "SICardRemoved";
		break;
	}
	case SIMessageData::Command::SICard5Detected: {
		logCardRead() << "SICard5Detected";
		setSiTask(new SiTaskReadCard5(false));
		break;
	}
	case SIMessageData::Command::GetSICard5: {
		logCardRead() << "GetSICard5";
		setSiTask(new SiTaskReadCard5(true));
		processSIMessageData(data);
		break;
	}
	case SIMessageData::Command::SICard6Detected: {
		logCardRead() << "SICard6Detected";
		setSiTask(new SiTaskReadCard6(false));
		break;
	}
	case SIMessageData::Command::GetSICard6: {
		logCardRead() << "GetSICard6";
		setSiTask(new SiTaskReadCard6(true));
		processSIMessageData(data);
		break;
	}
	case SIMessageData::Command::SICard8Detected: {
		logCardRead() << "SICard8AndHigherDetected";
		setSiTask(new SiTaskReadCard8(false));
		break;
	}
	case SIMessageData::Command::GetSICard8: {
		logCardRead() << "GetSICard8AndHigher";
		setSiTask(new SiTaskReadCard8(true));
		processSIMessageData(data);
		break;
	}
	default:
		qfError() << "unsupported command" << QString::number((uint8_t)cmd, 16);
	}
}

namespace
{
const char STX = 0x02;
const char ETX = 0x03;
const char ACK = 0x06;
const char NAK = 0x15;
//static const char DLE = 0x10;
}

void DeviceDriver::processData(const QByteArray &data)
{
	qfLogFuncFrame() << "\n" << SIMessageData::dumpData(data, 16);
	f_rxData.append(data);
	while(f_rxData.size() > 3) {
		int stx_pos = f_rxData.indexOf(STX);
		if(stx_pos > 0)
			qfWarning() << tr("Garbage received, stripping %1 characters from beginning of buffer").arg(stx_pos);
		// remove multiple STX, this can happen
		while(stx_pos < f_rxData.size()-1 && f_rxData[stx_pos+1] == STX) {
			stx_pos++;
		}
		if(stx_pos > 0) {
			f_rxData = f_rxData.mid(stx_pos);
		}
		// STX,CMD,LEN, data, CRC1,CRC0,ETX/NAK
		if(f_rxData.size() < 3) // STX,CMD,LEN
			return;
		int len = (uint8_t)f_rxData[2];
		len += 3 + 3;
		if(f_rxData.size() < len)
			return;
		QByteArray data = f_rxData.mid(0, len);
		f_rxData = f_rxData.mid(len);
		auto etx = (uint8_t)data[data.size() - 1];
		if(etx == NAK) {
			emitDriverInfo(NecroLog::Level::Error, tr("NAK received"));
		}
		else if(etx == ETX) {
			auto cmd = (uint8_t)data[1];
			if(cmd < 0x80) {
				emitDriverInfo(NecroLog::Level::Error, tr("Legacy protocol is not supported, switch station to extended one."));
			}
			else {
				processSIMessageData(data);
			}
		}
		else {
			qfWarning() << tr("Valid message shall end with ETX or NAK, throwing data away");
		}
	}
}

void DeviceDriver::emitDriverInfo( NecroLog::Level level, const QString& msg )
{
	//qfLog(level) << msg;
	emit driverInfo(level, msg);
}

void DeviceDriver::sendCommand(int cmd, const QByteArray& data)
{
	qfLogFuncFrame();
	if(cmd < 0x80) {
		emitDriverInfo(NecroLog::Level::Error, tr("SIDeviceDriver::sendCommand() - ERROR Sending of EXT commands only is supported for sending."));
	}
	else {
		QByteArray ba;
		ba.resize(3);
		int len = data.length();
		set_byte_at(ba, 0, STX);
		set_byte_at(ba, 1, (char)cmd);
		set_byte_at(ba, 2, (char)len);

		ba += data;

		int crc_sum = crc(len + 2, reinterpret_cast<const unsigned char*>(ba.constData()) + 1);
		set_byte_at(ba, ba.length(), (crc_sum >> 8) & 0xFF);
		set_byte_at(ba, ba.length(), crc_sum & 0xFF);
		set_byte_at(ba, ba.length(), ETX);
		qfDebug() << "sending command:" << SIMessageData::dumpData(ba, 16);
		//f_commPort->write(ba);
		//f_rxTimer->start();
		emit dataToSend(ba);
	}
}

void DeviceDriver::setSiTask(SiTask *task)
{
	if(m_currentTask) {
		qfError() << "There is other command in progress already. It will be aborted and deleted.";
		m_currentTask->abort();
	}
	m_currentTask = task;
	SiTask::Type task_type = task->type();
	connect(task, &SiTask::sigSendCommand, this, &DeviceDriver::sendCommand);
	connect(task, &SiTask::sigSendACK, this, &DeviceDriver::sendACK);
	connect(task, &SiTask::aboutToFinish, this, [this]() {
		this->m_currentTask = nullptr;
	});
	connect(task, &SiTask::finished, this, [this, task_type](bool ok, QVariant result) {
		if(ok) {
			emit this->siTaskFinished(static_cast<int>(task_type), result);
		}
	});
	m_currentTask->start();
}

void DeviceDriver::sendACK()
{
	emit dataToSend(QByteArray(1, ACK));
}

}
