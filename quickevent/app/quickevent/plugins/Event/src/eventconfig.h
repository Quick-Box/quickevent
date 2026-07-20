#pragma once

#include <QDate>
#include <QMap>
#include <QString>
#include <QTime>
#include <QVariantMap>

namespace Event {

struct EventConfig
{
	enum class Sport {OB = 1, LOB, MTBO, TRAIL};
	enum class Discipline {LongDistance = 1,
						   ShortDistance = 2,
						   Sprint = 3,
						   UltralongDistance = 4,
						   Relays = 5,
						   Teams = 6,
						   FreeOrder = 7,
						   NightRace = 9,
						   TempO = 11,
						   MultiStages = 13,
						   MassStart = 14,
						   SprintRelays = 15,
						   KnocOutSprint = 16,
						   Indoor = 19,
						  };

	static std::optional<Discipline> disciplineFromInt(int i);

	bool isHandicap() const {return handicapLength > 0;}
	std::optional<int> maximumCardCheckAdvanceSec() const;

	bool isRelays() const;

	static EventConfig fromVariantMap(const QVariantMap &values);
	QVariantMap toVariantMap() const;

	int stageCount = 1;
	QString name;
	QDate date;
	QTime time;
	QString description;
	QString place;
	QString mainReferee;
	QString director;
	int handicapLength = 0;
	int sportId = 0;
	int disciplineId = 0;
	int importId = 0;
	QString orisEventKey;
	int cardCheckTimeSec = 0;
	int oneTenthSecResults = 0;
	bool iofRace = false;
	int iofXmlRaceNumber = 0;
	int currentStageId = 1;
};

}
