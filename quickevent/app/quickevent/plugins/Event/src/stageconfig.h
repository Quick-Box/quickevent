#pragma once

#include <QDateTime>
#include <QString>
#include <QVariantMap>
#include <vector>

namespace Event {

struct StartSlotConfig
{
	static StartSlotConfig fromVariantMap(const QVariantMap &map);
	QVariantMap toVariantMap() const;

	int startOffset = 0;
	bool ignoreClassClashCheck = false;
};

struct DrawingConfig
{
	static DrawingConfig fromString(const QString& str);
	static DrawingConfig fromVariantMap(const QVariantMap &map);
	QVariantMap toVariantMap() const;
	const StartSlotConfig &startSlotConfig(int index) const;
	StartSlotConfig startSlotConfig(int index, const QString &start_code) const;
	void setStartSlots(const QString &start_code, const std::vector<StartSlotConfig> &slots_vec);
	std::vector<StartSlotConfig> startSlots;
	QVariantMap startLayouts;
};

struct StageConfig
{
	static StageConfig fromVariantMap(const QVariantMap &map);
	QVariantMap toVariantMap() const;

	QDateTime startDateTime;
	bool useAllMaps = false;
	DrawingConfig drawingConfig;
};

} // namespace Event
