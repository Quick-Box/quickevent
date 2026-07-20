#include "stageconfig.h"

#include <qf/core/utils.h>
#include <qf/core/log.h>

#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>

namespace Event {

StartSlotConfig StartSlotConfig::fromVariantMap(const QVariantMap& map)
{
    StartSlotConfig config;
    config.startOffset = map.value("startOffset").toInt();
    config.ignoreClassClashCheck = map.value("ignoreClassClashCheck").toBool();
    return config;
}

QVariantMap StartSlotConfig::toVariantMap() const
{
    QVariantMap map;
    map["startOffset"] = startOffset;
    map["ignoreClassClashCheck"] = ignoreClassClashCheck;
    return map;
}

DrawingConfig DrawingConfig::fromString(const QString& str)
{
    DrawingConfig config;
    QJsonParseError parseError;
    QJsonDocument doc = QJsonDocument::fromJson(str.toUtf8(), &parseError);
    if (parseError.error == QJsonParseError::NoError) {
        if (doc.isObject()) {
            config = fromVariantMap(doc.object().toVariantMap());
        }
    } else {
        qfWarning() << "Failed to parse drawing config JSON:" << parseError.errorString();
    }
    return config;
}

DrawingConfig DrawingConfig::fromVariantMap(const QVariantMap& map)
{
    DrawingConfig config;
    auto lst = map.value("startSlots").toList();
    for (const auto& slot : lst) {
        config.startSlots.push_back(StartSlotConfig::fromVariantMap(slot.toMap()));
    }
    return config;
}

QVariantMap DrawingConfig::toVariantMap() const
{
    QVariantMap map;
    QVariantList lst;
    for (const auto& slot : startSlots) {
        lst.append(slot.toVariantMap());
    }
    map["startSlots"] = lst;
    return map;
}

const StartSlotConfig& DrawingConfig::startSlotConfig(int index) const
{
    if (index < 0 || index >= static_cast<int>(startSlots.size())) {
        static StartSlotConfig defaultSlot;
        return defaultSlot;
    }
    return startSlots[index];
}

StageConfig StageConfig::fromVariantMap(const QVariantMap& map)
{
    StageConfig config;
    config.startDateTime = map.value("startDateTime").toDateTime();
    config.useAllMaps = map.value("useAllMaps").toBool();
    auto v = map.value("drawingConfig");
    if (v.userType() == QMetaType::QVariantMap) {
        config.drawingConfig = DrawingConfig::fromVariantMap(v.toMap());
    }
    else if (v.userType() == QMetaType::QString) {
        config.drawingConfig = DrawingConfig::fromString(v.toString());
    }
    return config;
}

QVariantMap StageConfig::toVariantMap() const
{
    QVariantMap map;
    map["startDateTime"] = startDateTime;
    map["useAllMaps"] = useAllMaps;
    map["drawingConfig"] = drawingConfig.toVariantMap();
    return map;
}

}
