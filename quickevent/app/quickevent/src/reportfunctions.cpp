#include "reportfunctions.h"

#include <quickevent/core/og/timems.h>

#include <qf/gui/framework/application.h>
#include <qf/gui/reports/processor/xmlreportdocument.h>

#include <QCoreApplication>

using qf::gui::reports::XmlReportDocument;
using quickevent::core::og::TimeMs;

namespace {

// translations are shared with former ogtime.js
QString trOgTime(const char *text)
{
	return QCoreApplication::translate("ogtime", text);
}

bool isEmptyValue(const QVariant &v)
{
	return !v.isValid() || v.isNull() || (v.typeId() == QMetaType::QString && v.toString().isEmpty());
}

// msec since midnight as h:mm:ss, hours are not wrapped
QString msecToHhmmss(qint64 msec)
{
	if(msec < 0)
		return QString::number(msec / 1000.);
	qint64 n = msec / 1000;
	int sec = n % 60;
	n /= 60;
	int min = n % 60;
	qint64 hod = n / 60;
	return QStringLiteral("%1:%2:%3").arg(hod).arg(min, 2, 10, QChar('0')).arg(sec, 2, 10, QChar('0'));
}

int timeMsColumnWidth(int time_prec)
{
	switch (static_cast<quickevent::core::og::TimeMeasurementPrecision>(time_prec)) {
	case quickevent::core::og::TimeMeasurementPrecision::Second: break;
	case quickevent::core::og::TimeMeasurementPrecision::MSec100: return 16;
	case quickevent::core::og::TimeMeasurementPrecision::MSec10: return 18;
	case quickevent::core::og::TimeMeasurementPrecision::MSec1: return 20;
	}
	return 13;
}

}

namespace quickevent {

void registerReportFunctions()
{
	// time in msec as mm.ss, argument (if present) is used for unreal times
	XmlReportDocument::registerFormatter("ogTime", [](const QVariant &v, const QString &arg) -> QVariant {
		if(isEmptyValue(v))
			return QString();
		int msec = v.toInt();
		if(!arg.isNull() && msec >= TimeMs::UNREAL_TIME_MSEC)
			return arg;
		return TimeMs(msec).toString();
	});
	// time in msec as mm.ss with second precision
	XmlReportDocument::registerFormatter("ogTimeSec", [](const QVariant &v, const QString &) -> QVariant {
		if(isEmptyValue(v))
			return QString();
		return TimeMs(v.toInt()).toString(quickevent::core::og::TimeMeasurementPrecision::Second);
	});
	// msec since midnight as h:mm:ss
	XmlReportDocument::registerFormatter("hhmmss", [](const QVariant &v, const QString &) -> QVariant {
		if(isEmptyValue(v))
			return QString();
		return msecToHhmmss(v.toLongLong());
	});
	// SI card time in msec as h:mm:ss, missing time is printed as --:--:--
	XmlReportDocument::registerFormatter("siTime", [](const QVariant &v, const QString &) -> QVariant {
		constexpr qint64 SI_NO_TIME_MSEC = 0xeeee * 1000;
		qint64 msec = v.toLongLong();
		if(msec == SI_NO_TIME_MSEC)
			return QStringLiteral("--:--:--");
		return msecToHhmmss(msec);
	});
	// time in msec as mm.ss or run status for special time values
	XmlReportDocument::registerFormatter("ogTimeStatus", [](const QVariant &v, const QString &) -> QVariant {
		if(isEmptyValue(v))
			return QString();
		switch (v.toInt()) {
		case TimeMs::DISQ_TIME_MSEC: return trOgTime(QT_TRANSLATE_NOOP("ogtime", "DISQ"));
		case TimeMs::MISPUNCH_TIME_MSEC: return trOgTime(QT_TRANSLATE_NOOP("ogtime", "MP"));
		case TimeMs::NOT_COMPETITING_TIME_MSEC: return trOgTime(QT_TRANSLATE_NOOP("ogtime", "NC"));
		case TimeMs::NOT_FINISH_TIME_MSEC: return trOgTime(QT_TRANSLATE_NOOP("ogtime", "DNF"));
		case TimeMs::NOT_START_TIME_MSEC: return trOgTime(QT_TRANSLATE_NOOP("ogtime", "DNS"));
		case TimeMs::OVERTIME_TIME_MSEC: return trOgTime(QT_TRANSLATE_NOOP("ogtime", "OVRT"));
		default: return TimeMs(v.toInt()).toString();
		}
	});
	// time column width in mm for time measurement precision
	XmlReportDocument::registerFormatter("ogTimeColumnWidth", [](const QVariant &v, const QString &) -> QVariant {
		return timeMsColumnWidth(v.toInt());
	});
	// run status from current row flags
	XmlReportDocument::registerFunction("ogRunStatus", [](const XmlReportDocument::ExpressionContext &ctx, const QVariantList &) -> QVariant {
		auto flag = [&ctx](const char *name) { return ctx.field(name).toBool(); };
		if(flag("notCompeting"))
			return trOgTime(QT_TRANSLATE_NOOP("ogtime", "NC"));
		if(flag("disqualified")) {
			if(flag("disqualifiedByOrganizer"))
				return trOgTime(QT_TRANSLATE_NOOP("ogtime", "DISQ"));
			if(flag("misPunch"))
				return trOgTime(QT_TRANSLATE_NOOP("ogtime", "MP"));
			if(flag("notStart"))
				return trOgTime(QT_TRANSLATE_NOOP("ogtime", "DNS"));
			if(flag("notFinish"))
				return trOgTime(QT_TRANSLATE_NOOP("ogtime", "DNF"));
			if(flag("overTime"))
				return trOgTime(QT_TRANSLATE_NOOP("ogtime", "OVRT"));
			return trOgTime(QT_TRANSLATE_NOOP("ogtime", "DISQ"));
		}
		return QString();
	});
	// "E<stage> " prefix of report title for multi-stage events
	XmlReportDocument::registerFunction("qeStagePrefix", [](const XmlReportDocument::ExpressionContext &ctx, const QVariantList &) -> QVariant {
		int stage_cnt = ctx.parameter("eventConfig.stageCount").toInt();
		int stage_id = ctx.parameter("stageId").toInt();
		if(stage_cnt > 1 && stage_id > 0)
			return QStringLiteral("E%1 ").arg(stage_id);
		return QString();
	});
	XmlReportDocument::registerFunction("appVersion", [](const XmlReportDocument::ExpressionContext &, const QVariantList &) -> QVariant {
		auto *app = qobject_cast<qf::gui::framework::Application*>(QCoreApplication::instance());
		return app? app->versionString(): QCoreApplication::applicationVersion();
	});
}

}
