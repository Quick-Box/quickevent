#include "classdef.h"

namespace quickevent::core {

QString ClassDef::startModeToString(StartMode mode)
{
	switch(mode) {
	case StartMode::StartList:
		return QStringLiteral("StartList");
	case StartMode::MassStart:
		return QStringLiteral("MassStart");
	case StartMode::PursuitStart:
		return QStringLiteral("PursuitStart");
	case StartMode::WaveStart:
		return QStringLiteral("WaveStart");
	case StartMode::FreeStart:
		return QStringLiteral("FreeStart");
	case StartMode::Auto:
		break;
	}
	return {};
}

ClassDef::StartMode ClassDef::startModeFromString(const QString &str)
{
	if(str == QLatin1String("StartList")) {
		return StartMode::StartList;
	}
	if(str == QLatin1String("MassStart")) {
		return StartMode::MassStart;
	}
	if(str == QLatin1String("PursuitStart")) {
		return StartMode::PursuitStart;
	}
	if(str == QLatin1String("WaveStart")) {
		return StartMode::WaveStart;
	}
	if(str == QLatin1String("FreeStart")) {
		return StartMode::FreeStart;
	}
	return StartMode::Auto;
}

QString ClassDef::resultListModeToString(ResultListMode mode)
{
	switch(mode) {
	case ResultListMode::Unordered:
		return QStringLiteral("Unordered");
	case ResultListMode::UnorderedNoTimes:
		return QStringLiteral("UnorderedNoTimes");
	case ResultListMode::Default:
		break;
	}
	return QStringLiteral("Default");
}

ClassDef::ResultListMode ClassDef::resultListModeFromString(const QString &str)
{
	if(str == QLatin1String("Unordered")) {
		return ResultListMode::Unordered;
	}
	if(str == QLatin1String("UnorderedNoTimes")) {
		return ResultListMode::UnorderedNoTimes;
	}
	return ResultListMode::Default;
}

} // namespace quickevent::core
