#include "cardcheckerfreeordercpp.h"

#include <quickevent/core/codedef.h>
#include <quickevent/core/coursedef.h>
#include <quickevent/core/og/timems.h>
#include <quickevent/core/si/punchrecord.h>
#include <quickevent/core/si/readcard.h>

#include <qf/core/log.h>

using quickevent::core::og::quantizeTimeMsec;
using quickevent::core::og::TimeMs;

namespace CardReader {

CardCheckerFreeOrderCpp::CardCheckerFreeOrderCpp(QObject *parent)
	: Super(parent)
{
	setObjectName("FreeOrder");
	setCaption(tr("Free order race"));
}

quickevent::core::si::CheckedCard CardCheckerFreeOrderCpp::checkCard(const quickevent::core::si::ReadCard &read_card)
{
	qfDebug() << "read card:" << read_card.toString();

	int run_id = read_card.runId();
	quickevent::core::CourseDef course;
	if(run_id > 0)
		course = courseCodesForRunId(run_id);

	quickevent::core::si::CheckedCard checked_card;
	if(course.isEmpty())
		return checked_card;

	checked_card.setCourseId(course.id());
	checked_card.setRunId(run_id);
	int stage_id = stageIdForRun(run_id);

	bool error_mis_punch = false;

	QList<quickevent::core::si::CheckedPunch> checked_punches;
	QVariantList course_codes = course.value(QStringLiteral("codes")).toList();
	QVariantMap finish_code = course.value(QStringLiteral("finishCode")).toMap();
	QList<quickevent::core::si::ReadPunch> read_punches = read_card.punchList();

	//........... normalize times .....................
	// checked card times are in msec relative to run start time
	// startTime, checkTime and finishTime in in msec relative to event start time 00
	int start00sec = stageStartSec(stage_id);
	checked_card.setStageStartTimeMs(start00sec * 1000);
	if(read_card.checkTime() != 0xEEEE) {
		// temporaryly shift start for 10 min to enable negative chceckTime for runners starting in 00 and 01
		static constexpr int MIN_10_MSEC = 10 * 60 * 1000;
		auto shifted_start00 = (start00sec * 1000) - MIN_10_MSEC;
		auto shifted_interval = msecIntervalAM(shifted_start00, read_card.checkTime() * 1000);
		checked_card.setCheckTimeMs(shifted_interval - MIN_10_MSEC);
		//checked_card.setCheckTimeMs(msecIntervalAM(start00sec * 1000, read_card.checkTime() * 1000));
	}
	if(read_card.startTime() == 0xEEEE) {        //take start record from start list
		if(run_id > 0) {
			checked_card.setStartTimeMs(startTimeSec(run_id) * 1000);
		}
	}
	else {
		checked_card.setStartTimeMs(msecIntervalAM(start00sec * 1000, read_card.startTime() * 1000));
	}

	if(read_card.finishTime() == 0xEEEE) {
		error_mis_punch = true;
	}
	else {
		checked_card.setFinishTimeMs(msecIntervalAM(start00sec * 1000, read_card.finishTime() * 1000));
		if(read_card.finishTimeMs()) {
			// add msec part of finish time
			checked_card.setFinishTimeMs(checked_card.finishTimeMs() + read_card.finishTimeMs());
		}
	}

	if(read_card.startTime() == 0xEEEE) {
		if(auto card_check_sec = maximumCardCheckAdvanceSec(); card_check_sec.has_value()) {
			if(read_card.checkTime() == 0xEEEE) {
				checked_card.setBadCheck(true);
			}
			else {
				auto max_card_check_diff_msec = card_check_sec.value() * 1000;
				int diff_msec = checked_card.startTimeMs() - checked_card.checkTimeMs();
				checked_card.setBadCheck(diff_msec > max_card_check_diff_msec);
			}
		}
	}

	//prepare list and map of course controls
	QMap<int, quickevent::core::si::CheckedPunch> map_of_control_codes;
	for(const auto &cp : course_codes) {
		quickevent::core::si::CheckedPunch checked_punch = quickevent::core::si::CheckedPunch::fromCodeDef(cp.toMap());
		map_of_control_codes.insert(checked_punch.code(), checked_punch);
	}

	int k;
	for(k=0; k<read_punches.length(); k++) { //scan card
		const quickevent::core::si::ReadPunch &read_punch = read_punches[k];

		qfDebug() << k << "looking for:" << read_punch.code() << " from list of codes";

		//TODO add possiblity to check alternative code
		if (map_of_control_codes.contains(read_punch.code())) {
		//found control code in list

			int read_punch_time_ms = read_punch.time() * 1000;
			if(read_punch.msec())
				read_punch_time_ms += read_punch.msec();

			quickevent::core::si::CheckedPunch checked_punch = map_of_control_codes.value(read_punch.code());
			checked_punch.setStpTimeMs(msecIntervalAM(checked_card.stageStartTimeMs() + checked_card.startTimeMs(), read_punch_time_ms));
			qfDebug() << read_punch.code() << "OK";

			//remove from list of course controls
			map_of_control_codes.remove(read_punch.code());

			checked_punches << checked_punch;

		} else //punched control code not found in course control list
		{
			qfDebug() << k << "NOT FOUND";
		}

	}

	checked_card.setMisPunch(error_mis_punch || !map_of_control_codes.isEmpty());

	quickevent::core::si::CheckedPunch finish_punch;
	if(!finish_code.isEmpty())
		finish_punch = quickevent::core::si::CheckedPunch::fromCodeDef(finish_code);
	// no finish split time when finish is not punched, interval to the not set finish time is nonsense
	if(read_card.finishTime() != 0xEEEE) {
		finish_punch.setStpTimeMs(msecIntervalAM(checked_card.startTimeMs(), checked_card.finishTimeMs()));
	}
	checked_punches << finish_punch;

	QVariant prev_stp_time_ms = 0;
	for(auto &checked_punch : checked_punches) {
		if(checked_punch.stpTimeMs()) {
			// quantize to event precision, so that printed lap times sum up to printed split times
			int stp_time_ms = quantizeTimeMsec(checked_punch.stpTimeMs(), TimeMs::defaultTimeMeasurementPrecision());
			if(prev_stp_time_ms.isValid()) {
				checked_punch.setLapTimeMs(stp_time_ms - prev_stp_time_ms.toInt());
			}
			prev_stp_time_ms = stp_time_ms;
		}
		else {
			prev_stp_time_ms = QVariant();
		}
	}
	{
		QVariantList lst;
		for(const quickevent::core::si::CheckedPunch &p : checked_punches)
			lst << p;
		checked_card.setPunches(lst);
	}
	qfDebug() << "check result:" << checked_card.toString();
	return checked_card;

}

} // namespace CardReader
