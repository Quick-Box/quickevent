#include "timems.h"

#include <qf/core/log.h>

#include <QDateTime>
#include <QString>
#include <QRegularExpression>



namespace quickevent::core::og {

TimeMeasurementPrecision TimeMs::m_defaultTimeMeasurementPrecision = TimeMeasurementPrecision::Second;

int timePrecisionMsec(TimeMeasurementPrecision precision)
{
	switch (precision) {
	case TimeMeasurementPrecision::MSec100: return 100;
	case TimeMeasurementPrecision::MSec10: return 10;
	case TimeMeasurementPrecision::MSec1: return 1;
	case TimeMeasurementPrecision::Second: return 1000;
	}
	return 1000;
}

int quantizeTimeMsec(int time_ms, TimeMeasurementPrecision precision)
{
	const int precision_ms = timePrecisionMsec(precision);
	return time_ms - time_ms % precision_ms;
}

QDateTime quantizeDatetimeMsec(QDateTime date_time, TimeMeasurementPrecision precision)
{
	const int precision_ms = timePrecisionMsec(precision);
	return date_time.addMSecs(-(date_time.time().msec() % precision_ms));
}

TimeMs::TimeMs()
	: m_msec(0), m_isValid(false)
{

}

TimeMs::TimeMs(int msec)
	: m_msec(msec), m_isValid(true)
{

}

void TimeMs::setDefaultTimeMeasurementPrecision(TimeMeasurementPrecision prec)
{
	m_defaultTimeMeasurementPrecision = prec;
}

TimeMs TimeMs::fromVariant(const QVariant &time_v)
{
	if(time_v.userType() == QMetaType::Int)
		return TimeMs(time_v.toInt());
	return TimeMs();
}

QString TimeMs::toString(QChar sec_sep, QChar msec_sep, TimeMeasurementPrecision prec) const
{
	if(!isValid())
		return QString();

	int msec = m_msec;
	bool is_neg = msec < 0;
	if(is_neg)
		msec = -msec;

	int ms  = msec % 1000;
	int sec = (msec / 1000) % 60;
	int min = msec / (60 * 1000);

	QString ret = QStringLiteral("%1%2%3")
		.arg(min)
		.arg(sec_sep)
		.arg(sec, 2, 10, QChar('0'));

	if(prec != TimeMeasurementPrecision::Second || !msec_sep.isNull()) {
		int digits = 0;
		switch (prec) {
		case TimeMeasurementPrecision::Second:  break;
		case TimeMeasurementPrecision::MSec100: digits = 1; break;
		case TimeMeasurementPrecision::MSec10:  digits = 2; break;
		case TimeMeasurementPrecision::MSec1:   digits = 3; break;
		}
		if(digits > 0) {
			ret += msec_sep;
			ret += QStringLiteral("%1").arg(ms, 3, 10, QChar('0')).left(digits);
		}
	}

	if(is_neg)
		ret.prepend('-');
	return ret;
}


QString TimeMs::toString() const
{
	return toString(m_defaultTimeMeasurementPrecision);
}

QString TimeMs::toString(TimeMeasurementPrecision prec) const
{
	return toString('.', '/', prec);
}

TimeMs TimeMs::fromString(const QString &time_str)
{
	if(time_str.isEmpty())
		return TimeMs();

	static const QRegularExpression re(R"(^(\d+)(?:[.:\-,](\d+)(?:\/(\d+))?)?$)");
	auto m = re.match(time_str.trimmed());
	if(!m.hasMatch()) {
		qfWarning() << "Invalid OGTime string" << time_str;
		return TimeMs();
	}
	int min  = m.captured(1).toInt();
	int sec  = m.captured(2).toInt();
	int msec = m.captured(3).toInt();

	return TimeMs(msec + ((sec + (min * 60)) * 1000));
}

int TimeMs::fixTimeWrapAM(int time1_msec, int time2_msec)
{
	constexpr int HR_12_MSEC = 12 * 60 * 60 * 1000;
	while(time2_msec < time1_msec)
		time2_msec += HR_12_MSEC;
	while(time1_msec <= time2_msec - HR_12_MSEC)
		time2_msec -= HR_12_MSEC;
	return time2_msec;
}

int TimeMs::msecIntervalAM(int from_time_msec, int to_time_msec)
{
	return fixTimeWrapAM(from_time_msec, to_time_msec) - from_time_msec;
}

void TimeMs::registerQVariantFunctions()
{
	static bool registered = false;
	if(!registered) {
		registered = true;
#if QT_VERSION_MAJOR < 6
		{
			bool ok = QMetaType::registerComparators<TimeMs>();
			if(!ok)
				qfError() << "Error registering comparators for quickevent::core::og::TimeMs!";
		}
#endif
		{
			bool ok = QMetaType::registerConverter<TimeMs, int>([](const TimeMs &t) -> int {return t.msec();});
			if(!ok)
				qfError() << "Error registering converter for quickevent::core::og::TimeMs!";
		}
	}
}

}
