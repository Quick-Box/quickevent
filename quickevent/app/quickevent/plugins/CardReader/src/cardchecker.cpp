#include "cardchecker.h"

#include "../../Event/src/eventplugin.h"
#include "../../Runs/src/runsplugin.h"

#include <quickevent/core/og/timems.h>
#include <quickevent/core/codedef.h>
#include <quickevent/core/si/punchrecord.h>

#include <qf/gui/framework/mainwindow.h>

#include <qf/core/assert.h>
#include <qf/core/log.h>
#include <qf/core/sql/query.h>
#include <qf/core/sql/querybuilder.h>

#include <QTime>

namespace qfs = qf::core::sql;
using qf::gui::framework::getPlugin;
using Event::EventPlugin;
using Runs::RunsPlugin;

namespace CardReader {

CardChecker::CardChecker(QObject *parent)
	: QObject(parent)
{
}

int CardChecker::fixTimeWrapAM(int time1_msec, int time2_msec)
{
	return quickevent::core::og::TimeMs::fixTimeWrapAM(time1_msec, time2_msec);
}

int CardChecker::msecIntervalAM(int time1_msec, int time2_msec)
{
	return quickevent::core::og::TimeMs::msecIntervalAM(time1_msec, time2_msec);
}

int CardChecker::toAMms(int time_msec)
{
	return fixTimeWrapAM(0, time_msec);
	//return SIMessageCardReadOut::toAMms(time_msec);
}

int CardChecker::toAM(int time_sec)
{
	return toAMms(time_sec * 1000) / 1000;
	//return SIMessageCardReadOut::toAM(time_sec);
}

int CardChecker::stageIdForRun(int run_id)
{
	return getPlugin<EventPlugin>()->stageIdForRun(run_id);
}

int CardChecker::stageStartSec(int stage_id)
{
	auto event_plugin = getPlugin<EventPlugin>();
	if(stage_id == 0)
		stage_id = event_plugin->currentStageId();
	int ret = event_plugin->stageStartMsec(stage_id);
	return ret / 1000;
}

int CardChecker::startTimeSec(int run_id)
{
	int ret = 0;
	qfs::QueryBuilder qb;
	qb.select2("runs", "startTimeMs")
			.from("runs")
			.where("runs.id=" QF_IARG(run_id));
	qfs::Query q;
	q.exec(qb.toString(), qf::core::Exception::Throw);
	if(q.next())
		ret = q.value(0).toInt() / 1000;
	else
		qfError() << "Cannot find runs record for id:" << run_id;
	return ret;
}

std::optional<int> CardChecker::maximumCardCheckAdvanceSec()
{
	return getPlugin<EventPlugin>()->eventConfig().maximumCardCheckAdvanceSec();
}

quickevent::core::CourseDef CardChecker::courseCodesForRunId(int run_id)
{
	return getPlugin<RunsPlugin>()->courseCodesForRunId(run_id);
}

}
