#include "eventconfig.h"

#include <qf/core/utils.h>

namespace Event {

std::optional<EventConfig::Discipline> EventConfig::disciplineFromInt(int i)
{
	switch (static_cast<Discipline>(i)) {
	case Discipline::LongDistance: return Discipline::LongDistance;
	case Discipline::ShortDistance: return Discipline::ShortDistance;
	case Discipline::Sprint: return Discipline::Sprint;
	case Discipline::Relays: return Discipline::Relays;
	case Discipline::Teams: return Discipline::Teams;
	case Discipline::NightRace: return Discipline::NightRace;
	case Discipline::SprintRelays: return Discipline::SprintRelays;
	case Discipline::UltralongDistance: return Discipline::UltralongDistance;
	case Discipline::FreeOrder: return Discipline::FreeOrder;
	case Discipline::KnocOutSprint: return Discipline::KnocOutSprint;
	case Discipline::TempO: return Discipline::TempO;
	case Discipline::MultiStages: return Discipline::MultiStages;
	case Discipline::MassStart: return Discipline::MassStart;
	case Discipline::Indoor: return Discipline::Indoor;
	}
	return {};
}

EventConfig EventConfig::fromVariantMap(const QVariantMap &values)
{
	EventConfig data;
	data.stageCount = values.value("stageCount", 1).toInt();
	data.name = values.value("name").toString();
	data.date = values.value("date").toDate();
	data.time = values.value("time").toTime();
	data.description = values.value("description").toString();
	data.place = values.value("place").toString();
	data.mainReferee = values.value("mainReferee").toString();
	data.director = values.value("director").toString();
	data.handicapLength = values.value("handicapLength").toInt();
	data.sportId = values.value("sportId").toInt();
	data.disciplineId = values.value("disciplineId").toInt();
	data.importId = values.value("importId").toInt();
	data.orisEventKey = values.value("orisEventKey").toString();
	data.cardCheckTimeSec = values.value("cardChechCheckTimeSec").toInt();
	data.oneTenthSecResults = values.value("oneTenthSecResults").toInt();
	data.iofRace = values.value("iofRace").toBool();
	data.iofXmlRaceNumber = values.value("iofXmlRaceNumber").toInt();
	data.currentStageId = values.value("currentStageId", 1).toInt();
	return data;
}

std::optional<int> EventConfig::maximumCardCheckAdvanceSec() const
{
	if(auto sec = cardCheckTimeSec; sec > 0)
		return sec;
	return {};
}

bool EventConfig::isRelays() const
{
	return disciplineId == 5 || disciplineId == 6 || disciplineId == 15;
}

QVariantMap EventConfig::toVariantMap() const
{
	QVariantMap values;
	values.insert("stageCount", stageCount);
	values.insert("name", name);
	values.insert("date", date);
	values.insert("time", time);
	values.insert("description", description);
	values.insert("place", place);
	values.insert("mainReferee", mainReferee);
	values.insert("director", director);
	values.insert("handicapLength", handicapLength);
	values.insert("sportId", sportId);
	values.insert("disciplineId", disciplineId);
	values.insert("importId", importId);
	values.insert("orisEventKey", orisEventKey);
	values.insert("cardChechCheckTimeSec", cardCheckTimeSec);
	values.insert("oneTenthSecResults", oneTenthSecResults);
	values.insert("iofRace", iofRace);
	values.insert("iofXmlRaceNumber", iofXmlRaceNumber);
	values.insert("currentStageId", currentStageId);
	return values;
}

}
