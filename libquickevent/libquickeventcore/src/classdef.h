#ifndef QUICKEVENT_CORE_CLASSDEF_H
#define QUICKEVENT_CORE_CLASSDEF_H

#include "quickeventcoreglobal.h"

#include <QString>

namespace quickevent::core {

class QUICKEVENTCORE_DECL_EXPORT ClassDef
{
public:
	// IOF XML start mode, Auto (NULL in classdefs) is derived from event discipline and start interval
	enum class StartMode {Auto, StartList, MassStart, PursuitStart, WaveStart, FreeStart};
	// IOF XML result list mode
	enum class ResultListMode {Default, Unordered, UnorderedNoTimes};

	// Auto is empty string
	static QString startModeToString(StartMode mode);
	// empty or unknown string is Auto
	static StartMode startModeFromString(const QString &str);
	static QString resultListModeToString(ResultListMode mode);
	// empty or unknown string is Default
	static ResultListMode resultListModeFromString(const QString &str);
};

} // namespace quickevent::core

#endif // QUICKEVENT_CORE_CLASSDEF_H
