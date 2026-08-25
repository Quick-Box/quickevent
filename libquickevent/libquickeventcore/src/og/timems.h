#ifndef QUICKEVENTCORE_OG_TIMEMS_H
#define QUICKEVENTCORE_OG_TIMEMS_H

#include "../quickeventcoreglobal.h"

#include <QChar>
#include <QMetaType>

class QDateTime;
class QString;

namespace quickevent::core::og {

enum class TimeMeasurementPrecision { Second = 0, MSec100, MSec10, MSec1 };

QUICKEVENTCORE_DECL_EXPORT int timePrecisionMsec(TimeMeasurementPrecision precision);
QUICKEVENTCORE_DECL_EXPORT int quantizeTimeMsec(int time_ms, TimeMeasurementPrecision precision);
QUICKEVENTCORE_DECL_EXPORT QDateTime quantizeDatetimeMsec(QDateTime date_time, TimeMeasurementPrecision precision);

class QUICKEVENTCORE_DECL_EXPORT TimeMs
{
public:
	static constexpr int UNREAL_TIME_MSEC = 9999 * 60 * 1000;
	static constexpr int NOT_FINISH_TIME_MSEC = UNREAL_TIME_MSEC;
	static constexpr int NOT_START_TIME_MSEC = NOT_FINISH_TIME_MSEC -1;
	static constexpr int NOT_COMPETITING_TIME_MSEC = NOT_START_TIME_MSEC - 1;
	static constexpr int DISQ_TIME_MSEC = NOT_COMPETITING_TIME_MSEC - 1;
	static constexpr int MISPUNCH_TIME_MSEC = DISQ_TIME_MSEC - 1;
	static constexpr int OVERTIME_TIME_MSEC = MISPUNCH_TIME_MSEC - 1;
	static constexpr int MAX_REAL_TIME_MSEC = OVERTIME_TIME_MSEC - 1;

	TimeMs();
	TimeMs(int msec);

	bool isValid() const {return m_isValid;}
	bool operator==(const TimeMs &o) const
	{
		if(!isValid())
			return !o.isValid();
		if(!o.isValid())
			return !isValid();
		return msec() == o.msec();
	}
	bool operator<(const TimeMs &o) const
	{
		if(!isValid())
			return o.isValid();
		if(!o.isValid())
			return false;
		return msec() < o.msec();
	}

	static TimeMs fromVariant(const QVariant &time_v);
	static TimeMs fromString(const QString &time_str);

	static void setDefaultTimeMeasurementPrecision(TimeMeasurementPrecision prec);
	static TimeMeasurementPrecision defaultTimeMeasurementPrecision() { return m_defaultTimeMeasurementPrecision; }

	QString toString() const;
	QString toString(TimeMeasurementPrecision prec) const;

	int msec() const {return isValid()? m_msec: 0;}

	/// while time2 < time1 add 12 hours to time2 and return it
	static int fixTimeWrapAM(int time1_msec, int time2_msec);
	static int msecIntervalAM(int from_time_msec, int to_time_msec);

	static void registerQVariantFunctions();

private:
	QString toString(QChar sec_sep, QChar msec_sep, TimeMeasurementPrecision prec) const;
private:
	int m_msec;
	bool m_isValid;

	static TimeMeasurementPrecision m_defaultTimeMeasurementPrecision;
};

}

Q_DECLARE_METATYPE(quickevent::core::og::TimeMs)

#endif // QUICKEVENTCORE_OG_TIMEMS_H
