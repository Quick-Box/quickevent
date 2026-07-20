#pragma once

#include <QDateTime>
#include <QString>
#include <QVariantMap>
#include <vector>

namespace Event {

struct StartSlotConfig
{
    static StartSlotConfig fromVariantMap(const QVariantMap& map);
    QVariantMap toVariantMap() const;

    int startOffset;
	bool ignoreClassClashCheck = false;
};

struct DrawingConfig
{
    static DrawingConfig fromString(const QString& str);
    static DrawingConfig fromVariantMap(const QVariantMap& map);
    QVariantMap toVariantMap() const;
    const StartSlotConfig& startSlotConfig(int index) const;

    std::vector<StartSlotConfig> startSlots;
};

struct StageConfig
{
    static StageConfig fromVariantMap(const QVariantMap& map);
    QVariantMap toVariantMap() const;

	QDateTime startDateTime;
	bool useAllMaps = false;
	DrawingConfig drawingConfig;
};

}
