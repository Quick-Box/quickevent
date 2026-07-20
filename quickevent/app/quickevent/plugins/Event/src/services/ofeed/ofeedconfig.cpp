#include "ofeedconfig.h"

namespace Event::services {

OFeedConfig::OFeedConfig()
{
    hostUrl = "https://api.orienteerfeed.com";
    changelogOrigin = "START";
    // lastChangelogCall = QDateTime::fromSecsSinceEpoch(0);
}

OFeedConfig OFeedConfig::fromVariantMap(const QVariantMap& map)
{
    OFeedConfig config;
    config.hostUrl = map.value("hostUrl", config.hostUrl).toString();
    config.eventId = map.value("eventId", config.eventId).toString();
    config.eventPassword = map.value("eventPassword", config.eventPassword).toString();
    config.changelogOrigin = map.value("changelogOrigin", config.changelogOrigin).toString();
    config.lastChangelogCall = map.value("lastChangelogCall", config.lastChangelogCall).toDateTime();
    config.runXmlValidation = map.value("runXmlValidation", config.runXmlValidation).toBool();
    config.runChangesProcessing = map.value("runChangesProcessing", config.runChangesProcessing).toBool();
    config.introTourShowed = map.value("introTourShowed", config.introTourShowed).toBool();
    return config;
}

QVariantMap OFeedConfig::toVariantMap() const
{
    QVariantMap map;
    map["hostUrl"] = hostUrl;
    map["eventId"] = eventId;
    map["eventPassword"] = eventPassword;
    map["changelogOrigin"] = changelogOrigin;
    map["lastChangelogCall"] = lastChangelogCall;
    map["runXmlValidation"] = runXmlValidation;
    map["runChangesProcessing"] = runChangesProcessing;
    map["introTourShowed"] = introTourShowed;
    return map;
}

}
