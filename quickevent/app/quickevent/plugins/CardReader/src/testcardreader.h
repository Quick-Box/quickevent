#pragma once

#include <necrolog/necrologlevel.h>

#include <QObject>

namespace siut { class DeviceDriver; }

namespace CardReader {

class TestCardReader : public QObject
{
	Q_OBJECT
public:
	TestCardReader(uint64_t card_number, QObject *parent = nullptr);

	static void testReadCard_8063069();
private:
	siut::DeviceDriver* siDriver();
	void sendData(const QByteArray &data);
	QByteArray cardBlock(int ix);
	void onSiTaskFinished(int task_type, QVariant result);
	void onDriverInfo(NecroLogLevel level, const QString &msg);
private:
	uint64_t m_cardNumber = 0;
};

} // namespace CardReader

