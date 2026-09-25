#include "sicard.h"

#include <qf/core/utils.h>

namespace siut {

// SiCardBatteryStatus

SiCardBatteryStatus SiCardBatteryStatus::fromVariantMap(const QVariantMap &m)
{
	SiCardBatteryStatus s;
	s.voltage          = m.value(QStringLiteral("voltage")).toDouble();
	s.referenceVoltage = m.value(QStringLiteral("referenceVoltage")).toDouble();
	s.isLow            = m.value(QStringLiteral("isLow")).toBool();
	s.replaceDate      = m.value(QStringLiteral("replaceDate")).toString();
	return s;
}

QVariantMap SiCardBatteryStatus::toVariantMap() const
{
	QVariantMap m;
	m[QStringLiteral("voltage")]          = voltage;
	m[QStringLiteral("referenceVoltage")] = referenceVoltage;
	m[QStringLiteral("isLow")]            = isLow;
	m[QStringLiteral("replaceDate")]      = replaceDate;
	return m;
}

// SICard

SICard SICard::fromVariantMap(const QVariantMap &m)
{
	SICard card;
	card.stationNumber = m.value(QStringLiteral("stationNumber")).toInt();
	card.cardNumber    = m.value(QStringLiteral("cardNumber")).toInt();
	card.checkTime     = m.value(QStringLiteral("checkTime")).toInt();
	card.startTime     = m.value(QStringLiteral("startTime")).toInt();
	card.finishTime    = m.value(QStringLiteral("finishTime")).toInt();
	card.finishTimeMs  = m.value(QStringLiteral("finishTimeMs")).toInt();
	for (const auto &v : m.value(QStringLiteral("punches")).toList())
		card.punches << SIPunch::fromVariantMap(v.toMap());
	if (m.contains(QStringLiteral("batteryStatus")))
		card.batteryStatus = SiCardBatteryStatus::fromVariantMap(m.value(QStringLiteral("batteryStatus")).toMap());
	card.generatedTestDataNote = m.value(QStringLiteral("generatedTestDataNote")).toString();
	return card;
}

QVariantMap SICard::toVariantMap() const
{
	QVariantMap m;
	m[QStringLiteral("stationNumber")] = stationNumber;
	m[QStringLiteral("cardNumber")]    = cardNumber;
	m[QStringLiteral("checkTime")]     = checkTime;
	m[QStringLiteral("startTime")]     = startTime;
	m[QStringLiteral("finishTime")]    = finishTime;
	m[QStringLiteral("finishTimeMs")]  = finishTimeMs;
	QVariantList punchList;
	for (const auto &p : punches)
		punchList << p.toVariantMap();
	m[QStringLiteral("punches")] = punchList;
	if (batteryStatus.has_value())
		m[QStringLiteral("batteryStatus")] = batteryStatus->toVariantMap();
	if (!generatedTestDataNote.isEmpty())
		m[QStringLiteral("generatedTestDataNote")] = generatedTestDataNote;
	return m;
}

static QString time_str(int _time)
{
	QString ret = "%1:%2:%3";
	if(_time == 0xEEEE) ret = "----";
	else {
		int time = SICard::toAM(_time);
		ret = ret.arg(time / (60*60)).arg(QString::number((time / 60) % 60), 2, '0').arg(QString::number(time % 60), 2, '0');
	}
	return ret;
}

QString SICard::toString() const
{
	QStringList sl;
	sl << tr("stationNumber: %1").arg(stationNumber);
	sl << tr("cardNumber: %1").arg(cardNumber);
	sl << tr("check: %1").arg(time_str(checkTime));
	sl << tr("start: %1").arg(time_str(startTime));
	sl << tr("finish: %1").arg(time_str(finishTime));
	if (batteryStatus.has_value())
		sl << tr("batteryStatus: %1").arg(qf::core::Utils::qvariantToJson(batteryStatus->toVariantMap()));
	for (int n = 0; n < punchCount(); ++n) {
		SIPunch p = punchAt(n);
		sl << ("   " + QString::number(n+1)).right(4) + ".\t" + QString::number(p.code) + "\t" + time_str(p.time);
	}
	return sl.join("\n");
}

int SICard::punchCount() const
{
	return punches.count();
}

SIPunch SICard::punchAt(int i) const
{
	return punches.value(i);
}

QList<SIPunch> SICard::punchList() const
{
	return punches;
}

bool SICard::isTimeValid(int time)
{
	return time != INVALID_SI_TIME;
}

int SICard::toAMms(int time_msec)
{
	constexpr int MSEC_12HR = 12 * 60 * 60 * 1000;
	int ret = time_msec;
	while(ret < 0)
		ret += MSEC_12HR;
	while(ret >= MSEC_12HR)
		ret -= MSEC_12HR;
	return ret;
}

int SICard::toAM(int time_sec)
{
	return toAMms(time_sec * 1000) / 1000;
}

}
