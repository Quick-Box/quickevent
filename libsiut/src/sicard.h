#ifndef SIUT_SICARD_H
#define SIUT_SICARD_H

#include "sipunch.h"

#include <QCoreApplication>
#include <QString>
#include <QList>
#include <QVariantMap>

#include <optional>

namespace siut {

struct SIUT_DECL_EXPORT SiCardBatteryStatus
{
	double voltage = 0.0;
	double referenceVoltage = 0.0;
	bool isLow = false;
	QString replaceDate;

	static SiCardBatteryStatus fromVariantMap(const QVariantMap &m);
	QVariantMap toVariantMap() const;
};

struct SIUT_DECL_EXPORT SICard
{
	Q_DECLARE_TR_FUNCTIONS(SICard)
public:
	using PunchList = QList<SIPunch>;
	static constexpr int INVALID_SI_TIME = 0xEEEE;

	int stationNumber = 0;
	int cardNumber = 0;
	int checkTime = 0;
	int startTime = 0;
	int finishTime = 0;
	int finishTimeMs = 0;
	PunchList punches;
	std::optional<SiCardBatteryStatus> batteryStatus;
	QString generatedTestDataNote;

	SICard() = default;
	SICard(int card_number) : cardNumber(card_number) {}

	static SICard fromVariantMap(const QVariantMap &m);
	QVariantMap toVariantMap() const;

	QString toString() const;

	int punchCount() const;
	SIPunch punchAt(int i) const;
	QList<SIPunch> punchList() const;

	static bool isTimeValid(int time);
	static int toAMms(int time_msec);
	static int toAM(int time_sec);
};

}

#endif // SICARD_H
