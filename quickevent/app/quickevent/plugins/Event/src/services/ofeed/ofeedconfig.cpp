#include "ofeedconfig.h"

namespace Event::services {

OFeedConfig::OFeedConfig()
{
    hostUrl = "https://api.orienteerfeed.com";
    // lastChangelogCall = QDateTime::fromSecsSinceEpoch(0);
}

OFeedConfig OFeedConfig::fromVariantMap(const QVariantMap& map)
{
    OFeedConfig config;
    config.hostUrl = map.value("hostUrl", config.hostUrl).toString();
    config.eventId = map.value("eventId", config.eventId).toString();
    config.eventPassword = map.value("eventPassword", config.eventPassword).toString();
    // fall back to the pre-rename keys, so settings of existing events are not lost
    config.lastStartChangelogCall = map.value("lastStartChangelogCall", map.value("lastChangelogCall", config.lastStartChangelogCall)).toDateTime();
    config.lastOfficeChangelogCall = map.value("lastOfficeChangelogCall", config.lastOfficeChangelogCall).toDateTime();
    config.runXmlValidation = map.value("runXmlValidation", config.runXmlValidation).toBool();
    config.runStartChangesProcessing = map.value("runStartChangesProcessing", map.value("runChangesProcessing", config.runStartChangesProcessing)).toBool();
    config.runOfficeChangesProcessing = map.value("runOfficeChangesProcessing", config.runOfficeChangesProcessing).toBool();
    config.introTourShowed = map.value("introTourShowed", config.introTourShowed).toBool();
    return config;
}

QVariantMap OFeedConfig::toVariantMap() const
{
    QVariantMap map;
    map["hostUrl"] = hostUrl;
    map["eventId"] = eventId;
    map["eventPassword"] = eventPassword;
    map["lastStartChangelogCall"] = lastStartChangelogCall;
    map["lastOfficeChangelogCall"] = lastOfficeChangelogCall;
    map["runXmlValidation"] = runXmlValidation;
    map["runStartChangesProcessing"] = runStartChangesProcessing;
    map["runOfficeChangesProcessing"] = runOfficeChangesProcessing;
    map["introTourShowed"] = introTourShowed;
    return map;
}

}
