#include "sitask.h"
#include "../message/simessagedata.h"

#include <qf/core/log.h>

#include <QDateTime>
#include <QTimer>

#define logCardRead() qfCInfo("CardRead")

namespace siut {

//=================================================
//             SiTask
//=================================================
SiTask::SiTask(QObject *parent)
	: Super(parent)
{
	m_rxTimer = new QTimer(this);
	m_rxTimer->setSingleShot(true);
	m_rxTimer->setInterval(5000);
	connect(m_rxTimer, &QTimer::timeout, this, [this]() {
		qfError() << this << "SiCommand timeout after" << (m_rxTimer->interval() / 1000.) << "sec.";
		this->abortWithMessage(tr("SiCommand timeout after %1 sec.").arg(m_rxTimer->interval() / 1000.));
	});
	m_rxTimer->start();
}

SiTask::~SiTask() = default;

void SiTask::finishAndDestroy(bool ok, QVariant result)
{
	//qfWarning() << __FUNCTION__ << this;
	m_rxTimer->stop();
	emit aboutToFinish();
	emit finished(ok, result);
	deleteLater();
}

void SiTask::sendACK()
{
	emit sigSendACK();
}

void SiTask::sendCommand(int cmd, const QByteArray &data)
{
	//qfInfo() << "restarting timer, active:" << m_rxTimer->isActive() << ", remaining" << m_rxTimer->remainingTime() << "msec";
	m_rxTimer->start();
	emit sigSendCommand(cmd, data);
}

//===============================================================
// CmdSetDirectRemoteMode
//===============================================================
SiTaskSetDirectRemoteMode::SiTaskSetDirectRemoteMode(Mode mode, QObject *parent)
	: Super(parent)
	, m_mode(mode)
{
}

void SiTaskSetDirectRemoteMode::start()
{
	QByteArray ba(1, ' ');
	ba[0] = (m_mode == Mode::Direct)? SIMessageData::MS_MODE_DIRECT: SIMessageData::MS_MODE_REMOTE;
	sendCommand(static_cast<int>(SIMessageData::Command::SetDirectRemoteMode), ba);
}

void SiTaskSetDirectRemoteMode::onSiMessageReceived(const SIMessageData &msg)
{
	bool ok = false;
	SIMessageData::Command cmd = msg.command();
	if(cmd == SIMessageData::Command::SetDirectRemoteMode) {
		QByteArray hdr = msg.data();
		auto ms_mode = (uint8_t)hdr[5];
		if(ms_mode == SIMessageData::MS_MODE_DIRECT) {
			logCardRead() << "SI station in DIRECT mode.";
			ok = (m_mode == Mode::Direct);
		}
		else if(ms_mode == SIMessageData::MS_MODE_REMOTE) {
			logCardRead() << "SI station in REMOTE mode.";
			ok = (m_mode == Mode::Remote);
		}
	}
	else {
		qfError() << "Invalid command:" << (int)cmd << "received";
	}
	finishAndDestroy(ok, (int)m_mode);
}

//===============================================================
// CmdStationInfo
//===============================================================
QString SiStationConfig::toString() const
{
	QString msg = tr(""
					 "Station number: {{StationNumber}}\n"
					 "Extended mode: {{ExtendedMode}}\n"
					 "Auto send: {{AutoSend}}\n"
					 "Handshake: {{HandShake}}\n"
					 "Password access: {{PasswordAccess}}\n"
					 "Read out after punch: {{ReadOutAfterPunch}}\n"
					 "");
	msg.replace("{{StationNumber}}", QString::number(stationNumber()));
	msg.replace("{{ExtendedMode}}", (flags() & (unsigned)siut::SiStationConfig::Flag::ExtendedMode)? tr("True"): tr("False"));
	msg.replace("{{AutoSend}}", (flags() & (unsigned)siut::SiStationConfig::Flag::AutoSend)? tr("True"): tr("False"));
	msg.replace("{{HandShake}}", (flags() & (unsigned)siut::SiStationConfig::Flag::HandShake)? tr("True"): tr("False"));
	msg.replace("{{PasswordAccess}}", (flags() & (unsigned)siut::SiStationConfig::Flag::PasswordAccess)? tr("True"): tr("False"));
	msg.replace("{{ReadOutAfterPunch}}", (flags() & (unsigned)siut::SiStationConfig::Flag::ReadOutAfterPunch)? tr("True"): tr("False"));
	return msg;
}

void SiTaskStationConfig::start()
{
	QByteArray ba;
	ba.append(0x74);
	ba.append(0x01);
	sendCommand(static_cast<int>(SIMessageData::Command::GetSystemData), ba);
}

namespace {
uint8_t as_byte(char c)
{
	return static_cast<uint8_t>(c);
}
}

void SiTaskStationConfig::onSiMessageReceived(const SIMessageData &msg)
{
	bool ok = false;
	SiStationConfig ret;
	SIMessageData::Command cmd = msg.command();
	if(cmd == SIMessageData::Command::GetSystemData) {
		QByteArray hdr = msg.data();
		unsigned int n = as_byte(hdr[2]);
		n = (n << 8) + as_byte(hdr[3]);
		ret.setStationNumber(n);
		unsigned flags = static_cast<uint8_t>(hdr[5]);
		ret.setFlags(flags);
		ok = true;
	}
	else {
		qfError() << "Invalid command:" << static_cast<int>(cmd) << "received";
	}
	finishAndDestroy(ok, ret);
}

//===============================================================
// CmdSetDirectRemoteMode
//===============================================================
SiTaskReadStationBackupMemory::SiTaskReadStationBackupMemory(QObject *parent)
	: Super(parent)
{
}

void SiTaskReadStationBackupMemory::start()
{
	logCardRead() << "SwitchToRemote";
	m_state = State::SwitchToRemote;
	auto *cmd = new SiTaskSetDirectRemoteMode(SiTaskSetDirectRemoteMode::Mode::Remote);
	connect(cmd, &SiTaskSetDirectRemoteMode::sigSendCommand, this, &SiTaskReadStationBackupMemory::sigSendCommand);
	connect(this, &SiTaskReadStationBackupMemory::siMessageForwarded, cmd, &SiTaskSetDirectRemoteMode::onSiMessageReceived);
	connect(cmd, &SiTaskSetDirectRemoteMode::finished, this, [this](bool ok, QVariant ) {
		if(ok) {
			logCardRead() << "ReadPointer";
			m_state = State::ReadPointer;
			QByteArray ba;
			ba.append(0x1C);
			ba.append(0x07);
			sendCommand(static_cast<int>(SIMessageData::Command::GetSystemData), ba);
		}
		else {
			abort();
		}
	});
	cmd->start();
}

void SiTaskReadStationBackupMemory::onSiMessageReceived(const SIMessageData &msg)
{
	if(m_state == State::SwitchToRemote) {
		emit siMessageForwarded(msg);
	}
	else if(m_state == State::ReadPointer) {
		SIMessageData::Command cmd = msg.command();
		if(cmd == SIMessageData::Command::GetSystemData) {
			QByteArray hdr = msg.data();
			m_stationNumber = (int)SIPunch::getUnsigned(hdr, 3, 2);
			int ix = 6;
			/// the 4 byte backup memory address pointer is part of the data string: EP3, EP2, xx, xx, xx, EP1, EP0
			m_memoryDataPointer = (uint8_t)hdr[ix++];
			m_memoryDataPointer = (m_memoryDataPointer << 8) +  (uint8_t)hdr[ix++];
			ix += 3;
			m_memoryDataPointer = (m_memoryDataPointer << 8) +  (uint8_t)hdr[ix++];
			m_memoryDataPointer = (m_memoryDataPointer << 8) +  (uint8_t)hdr[ix++];
			logCardRead() << "backup memory pointer:" << "0x" + QString::number(m_memoryDataPointer, 16);

			m_state = State::CheckOverflow;
			/// according to
			/// https://github.com/gaudenz/sireader/blob/master/sireader.py
			/// we should find ad addr 0x3D
			logCardRead() << "CheckOverflow";
			QByteArray ba;
			ba.append(0x3D);
			ba.append(0x01);
			sendCommand(static_cast<int>(SIMessageData::Command::GetSystemData), ba);
		}
		else {
			qfError() << "Invalid command:" << (int)cmd << "received";
			abort();
			return;
		}
	}
	else if(m_state == State::CheckOverflow) {
		SIMessageData::Command cmd = msg.command();
		if(cmd == SIMessageData::Command::GetSystemData) {
			QByteArray hdr = msg.data();
			bool is_overflow = hdr[6];
			//qfInfo() << msg.dump();
			logCardRead() << "is memory overflow:" << is_overflow;

			//m_blockCount = m_isOverflow? MEMORY_SIZE: (m_memoryDataPointer - MEMORY_START) / m_blockSize + 1;
			m_blockCount = (is_overflow? MEMORY_SIZE - MEMORY_START: m_memoryDataPointer - MEMORY_START) / m_blockSize + 1;

			if(m_blockCount == 0) {
				finishAndDestroy(true, createResult());
				return;
			}
			if(m_blockCount > 255) {
				qfError() << "Invalid block count:" << m_blockCount;
				abort();
				return;
			}

			logCardRead() << "ReadData";
			emit progress(m_progressPhase++, (int)m_blockCount);
			m_state = State::ReadData;
			QByteArray ba;
			if(is_overflow)
				m_readDataPointer = m_memoryDataPointer + 1;
			else
				m_readDataPointer = MEMORY_START;
			ba.append((char)((m_readDataPointer >> (2*8)) & 0xFF));
			ba.append((char)((m_readDataPointer >> (1*8)) & 0xFF));
			ba.append((char)((m_readDataPointer >> (0*8)) & 0xFF));
			ba.append((char)m_blockSize);
			sendCommand(0x81, ba);
		}
		else {
			qfError() << "Invalid command:" << (int)cmd << "received";
			abort();
		}
	}
	else if(m_state == State::ReadData) {
		int cmd = (int)msg.command();
		if(cmd == 0x81) {
			logCardRead() << "block of" << msg.data().length() << "bytes received, " << m_blockCount << "blocks to receive";
			logCardRead() << "read data pointer:" << QString::number(m_readDataPointer, 16) << "memory data pointer:" << QString::number(m_memoryDataPointer, 16) << "bytes to load:" << QString::number(m_memoryDataPointer - m_readDataPointer, 16);
			QByteArray ba = msg.data().mid(8, m_blockSize);
			if(m_readDataPointer < m_memoryDataPointer) {
				if(m_memoryDataPointer - m_readDataPointer < m_blockSize) {
					logCardRead() << "stripping last packet to len:" << (m_memoryDataPointer - m_readDataPointer);
					ba = ba.mid(0, m_memoryDataPointer - m_readDataPointer);
				}
			}
			//qfInfo() << SIMessageData::dumpData(ba, 16);
			m_data.append(ba);
			emit progress(m_progressPhase++, (int)m_blockCount);
			if(!--m_blockCount) {
				logCardRead() << "SwitchToDirect";
				m_state = State::SwitchToDirect;
				auto *cmd = new SiTaskSetDirectRemoteMode(SiTaskSetDirectRemoteMode::Mode::Direct);
				connect(cmd, &SiTaskSetDirectRemoteMode::sigSendCommand, this, &SiTaskReadStationBackupMemory::sigSendCommand);
				connect(this, &SiTaskReadStationBackupMemory::siMessageForwarded, cmd, &SiTaskSetDirectRemoteMode::onSiMessageReceived);
				connect(cmd, &SiTaskSetDirectRemoteMode::finished, this, [this](bool ok, QVariant ) {
					if(ok) {
						finishAndDestroy(true, createResult());
					}
					else {
						abort();
					}
				});
				cmd->start();
				return;
			}
			m_readDataPointer += m_blockSize;
			if(m_readDataPointer - MEMORY_START > MEMORY_SIZE)
				m_readDataPointer = MEMORY_START;
			{
				QByteArray ba;
				ba.append((char)((m_readDataPointer >> (2*8)) & 0xFF));
				ba.append((char)((m_readDataPointer >> (1*8)) & 0xFF));
				ba.append((char)((m_readDataPointer >> (0*8)) & 0xFF));
				ba.append((char)m_blockSize);
				sendCommand(0x81, ba);
			}
		}
		else {
			qfError() << "Invalid command:" << cmd << "received";
			abort();
			return;
		}
	}
	else if(m_state == State::SwitchToDirect) {
		emit siMessageForwarded(msg);
	}
}

QVariantMap SiTaskReadStationBackupMemory::createResult()
{
	QVariantMap ret;
	ret["stationNumber"] = m_stationNumber;
	QVariantList punches;
	/*
	storage order: SI2-SI1-SI0-DATE1-DATE0-TH-TL-MS
	SI2, SI1, SI0 3 bytes SI card number
	DATE1, DATE0 2 bytes date
	DATE1
		bit 7-2 - 6 bit year - 0-64 part of year
		bit 1-0 - bit 3-2 of 4bit-month 1-12
	DATE0
		bit 7-6 - bit 1-0 - part of 4bit month 1-12
		bit 5-1 - 5bit day of month 1-31
		bit 0 - am/pm halfday
	TH, TL 2 bytes 12h binary punching time
	MS 1 byte 8bit 1/256 of seconds
	*/
	int n = 0;
	const auto *cdata = reinterpret_cast<const uint8_t *>(m_data.constData());
	for (int i = 0; i < m_data.size(); ) {
		QVariantList row;
		int si = cdata[i++];
		si = (si << 8) + cdata[i++];
		si = (si << 8) + cdata[i++];
		row << si;

		uint8_t b = cdata[i++];
		int year = (b & 0b11111100) >> 2;
		year += 2000;
		int month = (b & 0b11) << 2;
		b = cdata[i++];
		month = month + ((b & 0b11000000) >> 6);
		int day = (b & 0b00111110) >> 1;
		bool is_pm = b & 1;
		int sec = cdata[i++];
		sec = (sec << 8) + cdata[i++];
		int h = sec / 3600;
		int ms = cdata[i++];
		char buff[64];
		bool card_error = h >= 12;
		if(card_error)
			h -= 12;
		if(is_pm)
			h += 12;
		int m = (sec / 60) % 60;
		int s = sec % 60;
		ms = ms * 1000 / 256;
		{
			snprintf(buff, sizeof(buff)
					 , "%04d-%02d-%02d %02d:%02d:%02d.%03d"
					 , year, month, day, h, m, s, ms);
			logCardRead() << ++n << "si:" << si << buff << (card_error? "CardErr": "");
		}
		row << QDateTime{{year, month, day}, {h, m, s, ms}};
		row << card_error;

		punches.insert(punches.length(), row);
	}
	ret["punches"] = punches;
	return ret;
}

//===============================================================
// SiTaskReadCard
//===============================================================
SiTaskReadCard::~SiTaskReadCard()
= default;

void SiTaskReadCard::finishAndDestroy(bool ok, QVariant result)
{
	if(ok) {
		logCardRead() << m_card.toString();
		if(!m_withAutosend)
			sendACK();
	}
	Super::finishAndDestroy(ok, result);
}

//===============================================================
// SiTaskReadCard5
//===============================================================
void SiTaskReadCard5::start()
{
	if(!m_withAutosend) {
		sendCommand((int)SIMessageData::Command::GetSICard5, QByteArray(1, 0x00));
	}
}

void SiTaskReadCard5::onSiMessageReceived(const SIMessageData &msg)
{
	SIMessageData::Command cmd = msg.command();
	if(cmd == SIMessageData::Command::GetSICard5) {
		//qfInfo() << msg.toString();
		const QByteArray data = msg.data();
		int base = 5;
		qfDebug() << "Card5 data";
		qfDebug() << SIMessageData::dumpData(data.mid(base), 16);
		int station_number = (int)SIPunch::getUnsigned(data, base - 2);
		int card_number = (int)SIPunch::getUnsigned(data, base + 4);
		int cs = (uint8_t)data[base + 6];
		if(cs > 1)
			card_number += 100000 * cs;
		int check_time = (int)SIPunch::getUnsigned(data, base + 0x19);
		int start_time = (int)SIPunch::getUnsigned(data, base + 0x13);
		int finish_time = (int)SIPunch::getUnsigned(data, base + 0x15);
		int punch_cnt = (uint8_t)data[base + 0x17];
		punch_cnt--;

		SICard::PunchList punches;
		int base1 = base + 0x20;
		// 5 x 6 records with times
		for(int i=0; i<30 && i<punch_cnt; i++) {
			int offset = (3*i) + (i/5) + 1;
			int code = (uint8_t)data[base1 + offset];
			int time = (int)SIPunch::getUnsigned(data, base1 + offset + 1);
			//qfInfo() << i << "->" << QString::number(offset, 16);
			punches << SIPunch(code, time);
		}
		// 1 x 6 records without times
		for(int i=30; i<36 && i<punch_cnt; i++) {
			int offset = 16*(i-30);
			int code = (uint8_t)data[base1 + offset];
			punches << SIPunch(code, SICard::INVALID_SI_TIME);
		}

		m_card.setStationNumber(station_number);
		m_card.setCardNumber(card_number);
		m_card.setCheckTime(check_time);
		m_card.setStartTime(start_time);
		m_card.setFinishTime(finish_time);
		m_card.setPunches(punches);
		//qfInfo() << "\n" << m_card.toString();
		finishAndDestroy(true, m_card);
	}
	else {
		qfError() << "Invalid command:" << "0x" + QString::number((int)cmd, 16) << "received";
		abort();
	}
}

//===============================================================
// SiTaskReadCard6
//===============================================================
SiTaskReadCard6::~SiTaskReadCard6()
= default;

void SiTaskReadCard6::start()
{
	if(!m_withAutosend)
		sendCommand((int)SIMessageData::Command::GetSICard6, QByteArray(1, 0x00));
}

void SiTaskReadCard6::onSiMessageReceived(const SIMessageData &msg)
{
	SIMessageData::Command cmd = msg.command();
	if(cmd == SIMessageData::Command::GetSICard6) {
		int base = 6;
		const QByteArray data = msg.data();
		int block_number = (uint8_t)data[base-1];
		logCardRead() << "Card6 data, block number:" << block_number;
		qfDebug() << SIMessageData::dumpData(data.mid(base), 4);
		if(block_number == 0) {
			int station_number = (int)SIPunch::getUnsigned(data, base - 3);
			int card_number = (int)SIPunch::getUnsigned(data, base + 11, 3);
			m_cardSerie = Card6;// static_cast<CardSerie>(((uint8_t)data[base + 0x18]) & 15);
			logCardRead() << "CS:" << m_cardSerie << cardSerieToString(m_cardSerie) << "SI:" << card_number;
			m_card.setStationNumber(station_number);
			m_card.setCardNumber(card_number);
			if(m_cardSerie == Card6 || m_cardSerie == Card6Star) {
				m_punchCnt = (int)SIPunch::getUnsigned(data, base + 18, 1);
				logCardRead() << "Punch cnt:" << m_punchCnt;
				int clear_time = SIPunch(data, base + 32).time();
				int check_time = SIPunch(data, base + 28).time();
				if(check_time == siut::SICard::INVALID_SI_TIME)
					check_time = clear_time;
				int start_time = SIPunch(data, base + 24).time();
				int finish_time = SIPunch(data, base + 20).time();
				m_card.setCardNumber(card_number);
				m_card.setCheckTime(check_time);
				m_card.setStartTime(start_time);
				m_card.setFinishTime(finish_time);

				if(!m_withAutosend) {
					sendCommand((int)SIMessageData::Command::GetSICard6, QByteArray(1, 0x06));
				}
			}
			else {
				qfError() << "block:" << block_number << "unsupported card serie:" << m_cardSerie << cardSerieToString(m_cardSerie);
				abort();
			}
		}
		else {
			if(block_number >= 1 && block_number <= 7) {
				if(m_card.cardNumber() > 0) {
					QVariantList punches = m_card.punches();
					int pcnt = punches.count();
					for (int i = 0; pcnt + i < m_punchCnt && i < 32; ++i) {
						SIPunch p(data, base + (i*4));
						//qfInfo() << "B1" << p.code();
						punches << p;
					}
					m_card.setPunches(punches);
					logCardRead() << punches.count() << "of" << m_punchCnt << "punches read.";
					if(m_withAutosend) {
						// it seems that we have to always expect blocks 0,6,7 in autosend mode
						if(m_card.punchCount() >= m_punchCnt) {
							logCardRead() << "All the punches in auto-send received, DONE";
							finishAndDestroy(true, m_card);
						}
					}
					else {
						if(m_card.punchCount() < m_punchCnt) {
							int bn = block_number + 1;
							if(bn >= 8)
								bn = 2;
							logCardRead() << "Asking for next block:" << bn;
							sendCommand((int)SIMessageData::Command::GetSICard6, QByteArray(1, (char)bn));
						}
						else {
							logCardRead() << "All the punches received, DONE";
							finishAndDestroy(true, m_card);
						}
					}
				}
				else {
					qfError() << "Card6 unexpected block number:" << block_number
							  << ", block #0 must be read first. See https://github.com/Quick-Event/quickbox/issues/732";
					if(m_withAutosend)
						qfError() << "Try to switch auto-send mode off.";
					abort();
				}
			}
			else {
				qfError() << "Card6 unexpected block number:" << block_number;
				abort();
			}
		}
	}
	else {
		qfError() << "Invalid command:" << "0x" + QString::number((int)cmd, 16) << "received";
		abort();
	}
}

const char *SiTaskReadCard6::cardSerieToString(SiTaskReadCard6::CardSerie cs)
{
	switch(cs) {
	case Card6: return "Card6";
	case Card6Star: return "Card6Star";
	case Invalid: return "Invalid";
	}
	return "Unknown";
}

//===============================================================
// SiTaskReadCard8
//===============================================================
SiTaskReadCard8::~SiTaskReadCard8() = default;

void SiTaskReadCard8::start()
{
	if(!m_withAutosend) {
		sendCommand((int)SIMessageData::Command::GetSICard8, QByteArray(1, 0x00));
	}
}
/*
CARD 10, block 0
00 0000 02 ef 83 00 01 00 85 df c1 9b ea ea ea ea 0c 0f
01 0010 9c d0 ee ee ee ee 8c a5 a6 3a 08 2c 09 31 0f 7b
02 0020 3a e3 0b 17 58 66 38 30 37 36 30 30 33 3b 4b 65
03 0030 41 72 20 73 2e 72 2e 6f 2e 3b 3b 3b 3b 3b 3b 3b
04 0040 3b 3b 3b ee ee ee ee ee ee ee ee ee ee ee ee ee
05 0050 ee ee ee ee ee ee ee ee ee ee ee ee ee ee ee ee
06 0060 ee ee ee ee ee ee ee ee ee ee ee ee ee ee ee ee
07 0070 ee ee ee ee ee ee ee ee ee ee ee ee ee ee ee ee
08 0080 ee ee ee ee ee ee 5b f8 03

SIAC, block 0
00 0000 02 ef 83 00 04 00 ad 49 13 9e ea ea ea ea 0d 03
01 0010 55 1b ee ee ee ee 8d da 6a 58 11 14 12 33 0f 7d
02 0020 86 13 07 18 3f 78 38 32 32 36 33 32 33 3b 48 2e
03 0030 53 2e 48 2e 20 53 70 6f 72 74 20 73 2e 72 2e 6f
04 0040 2e 3b 3b 3b 3b 3b 3b 3b 3b 3b 3b ee ee ee ee ee
05 0050 ee ee ee ee ee ee ee ee ee ee ee ee ee ee ee ee
06 0060 ee ee ee ee ee ee ee ee ee ee ee ee ee ee ee ee
07 0070 ee ee ee ee ee ee ee ee ee ee ee ee ee ee ee ee
08 0080 ee ee ee ee ee ee 0c b1 03

both have card serie == 15
 */
namespace {
enum class CardTypeByNumber {
	InvalidNumber,
	Siac,
	Card11,
	Card10,
	Card9,
	Card8,
	Card6,
	Card5,
	pCard,
};

CardTypeByNumber cardNumberToType(int number)
{
	if (number < 500000) {
		return CardTypeByNumber::Card5;
	}
	if (number < 1000000 || (number >= 2003000 && number < 2004000)) {
		return CardTypeByNumber::Card6;
	}
	if ((number >= 2000000 && number < 2003000) || (number >= 2004000 && number < 3000000)) {
		return CardTypeByNumber::Card8;
	}
	if (number >= 1000000 && number < 2000000) {
		return CardTypeByNumber::Card9;
	}
	if (number >= 7000000 && number < 8000000) {
		return CardTypeByNumber::Card10;
	}
	if (number >= 8000000 && number < 9000000) {
		return CardTypeByNumber::Siac;
	}
	if (number >= 9000000 && number < 10000000) {
		return CardTypeByNumber::Card11;
	}
	return CardTypeByNumber::InvalidNumber;
}
}
void SiTaskReadCard8::onSiMessageReceived(const SIMessageData &msg)
{
	SIMessageData::Command cmd = msg.command();
	if(cmd == SIMessageData::Command::GetSICard8) {
		int base = 6;
		const QByteArray data = msg.data();
		int block_number = (uint8_t)data[base-1];
		logCardRead() << "Card8 data, block number:" << block_number;
		qfDebug() << SIMessageData::dumpData(data.mid(base), 4);
		if(block_number == 0) {
			int station_number = (int)SIPunch::getUnsigned(data, base - 3);
			int card_number = (int)SIPunch::getUnsigned(data, base + 0x19, 3);
			m_cardSerie = static_cast<CardSerie>(((uint8_t)data[base + 0x18]) & 15);
			// qfInfo() << "CS:" << m_cardSerie;
			logCardRead() << "CS:" << m_cardSerie << cardSerieToString(m_cardSerie) << "SI:" << card_number;
			m_card.setStationNumber(station_number);
			m_card.setCardNumber(card_number);
			if(m_cardSerie == Card8 || m_cardSerie == Card9 || m_cardSerie == pCard || m_cardSerie == Siac) {
				m_punchCnt = (uint8_t)data[base + 0x16];
				logCardRead() << "Punch cnt:" << m_punchCnt;
				int check_time = SIPunch(data, base + 0x08).time();
				int start_time = SIPunch(data, base + 0x0c).time();
				int finish_time = SIPunch(data, base + 0x10).time();
				m_card.setCardNumber(card_number);
				m_card.setCheckTime(check_time);
				m_card.setStartTime(start_time);
				m_card.setFinishTime(finish_time);

				if(m_cardSerie == Card9) {
					base += 14 * 4;
					QVariantList punches = m_card.punches();
					for (int i = 0; i < m_punchCnt && i < 18; ++i) {
						SIPunch p(data, base + (i*4));
						//qfInfo() << "B0" << p.code();
						punches << p;
					}
					m_card.setPunches(punches);
					if((punches.count() == m_punchCnt) && !m_withAutosend)
						finishAndDestroy(true, m_card);
				}
				if(!m_withAutosend) {
					if(m_cardSerie == Card8)
						sendCommand((int)SIMessageData::Command::GetSICard8, QByteArray(1, 0x01));
					else if(m_cardSerie == Card9 && m_card.punches().count() < m_punchCnt)
						sendCommand((int)SIMessageData::Command::GetSICard8, QByteArray(1, 0x01));
					else if(m_cardSerie == pCard)
						sendCommand((int)SIMessageData::Command::GetSICard8, QByteArray(1, 0x01));
					else if(m_cardSerie == Siac) {
						if (cardNumberToType(m_card.cardNumber()) == CardTypeByNumber::Siac) {
							// Invoke battery voltage measurement
							// 02 EA 05 7E 05 05 05 05 B2 31 03 - EA - PROBABLY SIAC battery measurement request
							auto ba = QByteArray::fromHex("7E05050505");
							sendCommand((int)SIMessageData::Command::SiacMeasureBattery, ba);
						} else {
							// Card 10, 11
							sendCommand((int)SIMessageData::Command::GetSICard8, QByteArray(1, 0x04));
						}
					}
				}
			}
			else {
				qfError() << "block:" << block_number << "unsupported card serie:" << m_cardSerie << cardSerieToString(m_cardSerie);
				abort();
			}
		}
		else {
			if(m_cardSerie == Card8) {
				if(block_number == 1) {
					base += 8;
					QVariantList punches = m_card.punches();
					int pcnt = punches.count();
					for (int i = 0; pcnt + i < m_punchCnt && i < 30; ++i) {
						SIPunch p(data, base + (i*4));
						//qfInfo() << "B1" << p.code();
						punches << p;
					}
					m_card.setPunches(punches);
					//qfInfo() << "\n" << m_card.toString();
					finishAndDestroy(true, m_card);
				}
				else {
					qfError() << "Card8 unexpected block number:" << block_number;
					abort();
				}
			}
			else if(m_cardSerie == Card9) {
				if(block_number == 1) {
					QVariantList punches = m_card.punches();
					int pcnt = punches.count();
					for (int i = 0; pcnt + i < m_punchCnt && i < 32; ++i) {
						punches << SIPunch(data, base + (i*4));
					}
					m_card.setPunches(punches);
					//qfInfo() << "\n" << m_card.toString();
					finishAndDestroy(true, m_card);
				}
				else {
					qfError() << "Card8 unexpected block number:" << block_number;
					abort();
				}
			}
			else if(m_cardSerie == pCard) {
				if(block_number == 1) {
					base += 12*4;
					QVariantList punches = m_card.punches();
					int pcnt = punches.count();
					for (int i = 0; pcnt + i < m_punchCnt && i < 20; ++i) {
						punches << SIPunch(data, base + (i*4));
					}
					m_card.setPunches(punches);
					finishAndDestroy(true, m_card);
				}
				else {
					qfError() << "Card8 unexpected block number:" << block_number;
					abort();
				}
			}
			else if(m_cardSerie == Siac) {
				if(block_number == 3) {
					// read battery data
					SiCardBatteryStatus battery_status;
					int yy = (uint8_t)data[base + (0xf*4) + 0];
					int mm = (uint8_t)data[base + (0xf*4) + 1];
					int dd = (uint8_t)data[base + (0xf*4) + 2];
					QDate date(yy + 2000, mm, dd);
					battery_status.setReplaceDate(date.toString(Qt::ISODate));
					logCardRead().nospace() << "SIAC batery date: " << battery_status.replaceDate();
					auto hw_ver_1 = (uint8_t)data[base + (0x10*4) + 0];
					auto hw_ver_0 = (uint8_t)data[base + (0x10*4) + 1];
					auto sw_ver_1 = (uint8_t)data[base + (0x10*4) + 2];
					auto sw_ver_0 = (uint8_t)data[base + (0x10*4) + 3];
					logCardRead().nospace() << "HW ver: " << hw_ver_1 << '.' << hw_ver_0;
					logCardRead().nospace() << "SW ver: " << sw_ver_1 << '.' << sw_ver_0;
					auto mvbat = (uint8_t)data[base + (0x11*4) + 3];
					auto rbat = (uint8_t)data[base + (0x15*4) + 0];
					auto lbat = (uint8_t)data[base + (0x15*4) + 1];
					battery_status.setLow(lbat != 0xAA);
					battery_status.setVoltage(1.9 + 0.09 * mvbat);
					battery_status.setReferenceVoltage(1.9 + 0.09 * rbat);
					logCardRead().nospace() << "MVBAT: " << " 0x" << QString::number(mvbat, 16);
					logCardRead().nospace() << "RBAT : " << " 0x" << QString::number(rbat, 16);
					logCardRead().nospace() << "LBAT : " << " 0x" << QString::number(lbat, 16) << " " << (lbat == 0xAA? "OK": "LOW");
					m_card.setBatteryStatus(battery_status);

					sendCommand((int)SIMessageData::Command::GetSICard8, QByteArray(1, (char)(block_number + 1)));
				}
				else if(block_number >= 4 && block_number <= 7) {
					QVariantList punches = m_card.punches();
					int pcnt = punches.count();
					for (int i = 0; pcnt + i < m_punchCnt && i < 128/4; ++i)
						punches << SIPunch(data, base + (i*4));
					m_card.setPunches(punches);
					if(m_withAutosend) {
						if(block_number == 7) {
							finishAndDestroy(true, m_card);
						}
					}
					else {
						if(m_card.punchCount() < m_punchCnt) {
							sendCommand((int)SIMessageData::Command::GetSICard8, QByteArray(1, (char)(block_number + 1)));
						}
						else {
							finishAndDestroy(true, m_card);
						}
					}
				}
				else {
					qfError() << "Siac unexpected block number:" << block_number;
					abort();
				}
			}
			else {
				qfError() << "block:" << block_number << "unsupported card serie:" << m_cardSerie << cardSerieToString(m_cardSerie);
				abort();
			}
		}
	}
	else if (cmd == SIMessageData::Command::SiacMeasureBattery) {
		// continue SIAC card read out
		sendCommand((int)SIMessageData::Command::GetSICard8, QByteArray(1, 0x03));
	}
	else {
		qfError() << "Invalid command:" << "0x" + QString::number((int)cmd, 16) << "received";
		if(cmd == SIMessageData::Command::SICardRemoved)
			qfError() << "seems like SI card was removed in middle of read-out process";
		abort();
	}
}

const char *SiTaskReadCard8::cardSerieToString(SiTaskReadCard8::CardSerie cs)
{
	switch(cs) {
	case Card8: return "Card8";
	case Card9: return "Card9";
	case pCard: return "pCard";
	case tCard: return "tCard";
	case Siac: return "Siac";
	case Invalid: return "Invalid";
	}
	return "Unknown";
}

}
