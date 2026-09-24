#ifndef REPORTSSETTINGS_H
#define REPORTSSETTINGS_H

#include "settings.h"

class ReportsSettings : public Core::Settings
{
public:
	ReportsSettings() : Core::Settings("plugin/Core/reports") {}

	// SETTINGS_FIELD(QString, r, setR, eportsDirectory, "reportsDirectory", {});
};

#endif // REPORTSSETTINGS_H
