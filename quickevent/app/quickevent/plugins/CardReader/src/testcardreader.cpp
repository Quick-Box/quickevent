#include "testcardreader.h"

#include <siut/sidevicedriver.h>

#include <qf/core/log.h>

namespace CardReader {

TestCardReader::TestCardReader(uint64_t card_number, QObject *parent)
	: QObject(parent)
	, m_cardNumber(card_number)
{

}
namespace {
auto fromHex(const QString &s)
{
	auto s2 = s;
	s2.replace(' ', "");
	return QByteArray::fromHex(s2.toUtf8());
};
auto toHex(const QByteArray &ba)
{
	return QString::fromUtf8(ba.toHex());
};
}
void TestCardReader::testReadCard_8063069()
{
	qfInfo() << "testReadCard_8063069";
	auto *reader = new TestCardReader(8063069, nullptr);
	auto *sidriver = new siut::DeviceDriver(reader);
	connect(sidriver, &siut::DeviceDriver::dataToSend, reader, &TestCardReader::sendData);
	connect(sidriver, &siut::DeviceDriver::siTaskFinished, reader, &TestCardReader::onSiTaskFinished);
	connect(sidriver, &siut::DeviceDriver::driverInfo, reader, &TestCardReader::onDriverInfo);
	sidriver->processData(fromHex("02 e8060001 01020304 0000 03"));
}

siut::DeviceDriver *TestCardReader::siDriver()
{
	auto *drv = findChild<siut::DeviceDriver*>();
	Q_ASSERT(drv);
	return drv;
}

void TestCardReader::sendData(const QByteArray &data)
{
	qfInfo() << "send data" << toHex(data);
	// 02 EA 05 7E 05 05 05 05 B2 31 03 - EA - PROBABLY SIAC battery measurement request
	static auto siac_get_battery = QByteArray::fromHex("02 EA 05 7E 05 05 05 05 B2 31 03");
	static auto ACK = QByteArray::fromHex("06");
	if (data == ACK) {
		return;
	}
	if (data == siac_get_battery) {
		siDriver()->processData(fromHex("02 ea00 0000 03"));
		return;
	}
	if (data.size() > 5 && data[1] == '\xEF' && data[2] == '\x01') {
		auto ix = static_cast<unsigned char>(data[3]);
		auto block = cardBlock(ix);
		Q_ASSERT(!block.isEmpty());
		siDriver()->processData(block);
		return;
	}
	qfError() << "Invalid command";
	deleteLater();
}

QByteArray TestCardReader::cardBlock(int ix)
{
	if (m_cardNumber == 8063069) {
		static const QList<QByteArray> blocks = {
			fromHex(
			"02ef830001"
			"00"
			"F8 E7 5A 9E EA EA EA EA 0D 01 2A 78 EE EE EE EE"
			"0D 14 39 1B 0F 7F 10 09 0F 7B 08 5D 02 16 5A 10"
			"38 30 36 33 30 36 39 3B 56 41 56 52 59 53 20 43"
			"5A 20 73 2E 72 2E 6F 3B 3B 3B 3B 6D 61 72 74 69"
			"6E 2E 6D 61 72 65 6B 40 76 61 76 72 79 73 2E 63"
			"7A 3B 2B 34 32 30 20 35 37 37 20 31 33 31 20 36"
			"38 35 2C 2B 34 32 30 20 37 33 36 20 35 34 30 20"
			"34 39 34 3B 4C 75 68 61 63 6F 76 69 63 65 3B 4E"
			"07ac03"),
			fromHex(
			"02ef830001"
			"01"
			"61 64 72 61 7A 6E 69 20 33 30 33 3B 37 36 33 32"
			"36 3B 43 5A 45 3B 00 00 EE EE EE EE EE EE EE EE"
			"07 00 1A 07 EE EE EE EE 04 14 74 14 EB EB EB EB"
			"32 9E 02 00 32 11 02 00 32 87 32 4A 02 00 32 E7"
			"32 32 02 00 32 61 02 00 32 D6 02 00 32 42 32 2D"
			"0D 64 38 D5 0D 64 38 D6 EE EE EE EE EE EE EE EE"
			"EE EE EE EE EE EE EE EE EE EE EE EE EE EE EE EE"
			"EE EE EE EE EE EE EE EE EE EE EE EE EE EE EE EE"
			"07ac03"),
			{},
			fromHex(
			"02ef830001"
			"03"
			"EE EE EE EE EE EE EE EE EE EE EE EE EE EE EE EE"
			"EE EE EE EE EE EE EE EE EE EE EE EE EE EE EE EE"
			"EE EE EE EE EE EE EE EE 31 E4 EE EE EE EE EE EE"
			"10 07 03 84 0D 7C 2A AD 0D 01 2A 3B 16 02 03 01"
			"01 06 04 03 0A 11 00 06 00 A4 00 00 18 00 05 4A"
			"08 00 05 0A 06 6C 01 4A EE EE EE EE 8D 1F 39 1B"
			"CA CA AA CA CC CC AC CC CA AA AC AA CC CC AA AA"
			"73 69 61 63 FF FF FF FF 03 84 00 7E 06 31 00 00"
			"07ac03"),
			fromHex(
			"02ef830001"
			"04"
			"0D 86 2C CF 0D 86 2C D1 0D 86 2C D2 0D 86 2C D1"
			"0D 86 2C D2 0D 81 31 71 0D 81 31 72 0D 81 31 72"
			"0D 8B 37 92 0D 8B 37 93 0D 8B 37 94 0D 8B 37 93"
			"0D 8B 37 94 0D 8B 37 93 0D 8B 37 95 0D 64 38 D5"
			"EE EE EE EE EE EE EE EE EE EE EE EE EE EE EE EE"
			"EE EE EE EE EE EE EE EE EE EE EE EE EE EE EE EE"
			"EE EE EE EE EE EE EE EE EE EE EE EE EE EE EE EE"
			"EE EE EE EE EE EE EE EE EE EE EE EE EE EE EE EE"
			"07ac03"),
			{}, // 05
			{}, // 06
			{}, // 07
		};
		if (ix >= 0 && ix < blocks.size()) {
			return blocks[ix];
		}
	}
	qfError() << "Invalid block index";
	deleteLater();
	return {};
}

void TestCardReader::onSiTaskFinished(int task_type, QVariant result)
{
	auto tt = static_cast<siut::SiTask::Type>(task_type);
	if(tt == siut::SiTask::Type::CardRead) {
		siut::SICard card(result.toMap());
		if(card.isEmpty())
			QF_EXCEPTION("Empty card received");
		qfInfo() << card.toString();
	}
	else {
		qfError() << "Invalid task finished";
	}
	deleteLater();
}

void TestCardReader::onDriverInfo(NecroLogLevel level, const QString &msg)
{
	qfInfo() << "DRIVER INFO:" << static_cast<int>(level) << msg;
}

} // namespace CardReader
