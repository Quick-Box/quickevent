.pragma library

// Mirrors C++ enum quickevent::core::og::TimeMeasurementPrecision
var TimeMeasurementPrecision = {
	Second:  0,
	MSec100: 1,
	MSec10:  2,
	MSec1:   3
};

// Injected by ReportProcessor via QJSEngine::globalObject().setProperty()
// to match TimeMs::defaultTimeMeasurementPrecision() at engine-creation time.
var defaultTimeMeasurementPrecision = (typeof ogDefaultTimeMeasurementPrecision !== 'undefined')
	? ogDefaultTimeMeasurementPrecision
	: TimeMeasurementPrecision.Second;

function msecToString_mmss(msec, prec)
{
	if(msec < 0)
		return '-' + msecToString_mmss(-msec);
	var ms = msec % 1000;
	var sec = ((msec / 1000) >> 0) % 60;
	var min = (msec / (1000 * 60)) >> 0;
	var ret = min + ".";
	if(sec < 10)
		ret += '0';
	ret += sec;
	var digits = 0;

	if (typeof prec === 'undefined') {
		prec = defaultTimeMeasurementPrecision
	}

	switch (prec) {
	case TimeMeasurementPrecision.MSec100: digits = 1; break;
	case TimeMeasurementPrecision.MSec10:  digits = 2; break;
	case TimeMeasurementPrecision.MSec1:   digits = 3; break;
	}
	if(digits > 0) {
		var ms_str = (ms < 100 ? '0' : '') + (ms < 10 ? '0' : '') + ms;
		ret += '/' + ms_str.substring(0, digits);
	}
	return ret;
}

function fixTimeWrapAM(time1_msec, time2_msec)
{
	var hr12ms = 12 * 60 * 60 * 1000;
	while(time2_msec < time1_msec)
		time2_msec += hr12ms;
	while(time1_msec <= time2_msec - hr12ms)
		time2_msec -= hr12ms;
	return time2_msec;
}

function msecIntervalAM(time1_msec, time2_msec)
{
	return fixTimeWrapAM(time1_msec, time2_msec) - time1_msec;
}

function ogTimeToString(time_ms)
{
	//return "999.99";
	if(time_ms === DISQ_TIME_MSEC)
		return qsTr("DISQ");
	if(time_ms === MISPUNCH_TIME_MSEC)
		return qsTr("MP");
	if(time_ms === NOT_COMPETITING_TIME_MSEC)
		return qsTr("NC");
	if(time_ms === NOT_FINISH_TIME_MSEC)
		return qsTr("DNF");
	if(time_ms === NOT_START_TIME_MSEC)
		return qsTr("DNS");
	if(time_ms === OVERTIME_TIME_MSEC)
		return qsTr("OVRT");
	return msecToString_mmss(time_ms);
}

function detailRunStatusToString(detail)
{
	if(detail.rowData("notCompeting"))
		return qsTr("NC");
	if(detail.rowData("disqualified")) {
		if (detail.rowData("disqualifiedByOrganizer"))
			return qsTr("DISQ");
		if (detail.rowData("misPunch"))
			return qsTr("MP");
		if (detail.rowData("notStart"))
			return qsTr("DNS");
		if (detail.rowData("notFinish"))
			return qsTr("DNF");
		if (detail.rowData("overTime"))
			return qsTr("OVRT");
		return qsTr("DISQ");
	}
	return "";
}

var UNREAL_TIME_MSEC = 9999 * 60 * 1000;
var NOT_FINISH_TIME_MSEC = UNREAL_TIME_MSEC;
var NOT_START_TIME_MSEC = NOT_FINISH_TIME_MSEC -1;
var NOT_COMPETITING_TIME_MSEC = NOT_START_TIME_MSEC - 1;
var DISQ_TIME_MSEC = NOT_COMPETITING_TIME_MSEC - 1;
var MISPUNCH_TIME_MSEC = DISQ_TIME_MSEC - 1;
var OVERTIME_TIME_MSEC = MISPUNCH_TIME_MSEC - 1;
var MAX_REAL_TIME_MSEC = OVERTIME_TIME_MSEC - 1;

function timeMsColumnWidth(time_prec)
{
	switch (time_prec) {
	// case TimeMeasurementPrecision.Second: return 12;
	case TimeMeasurementPrecision.MSec100: return 16;
	case TimeMeasurementPrecision.MSec10:  return 18;
	case TimeMeasurementPrecision.MSec1:   return 20;
	}
	return 13
}
