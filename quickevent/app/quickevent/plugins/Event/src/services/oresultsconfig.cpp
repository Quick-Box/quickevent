#include "oresultsconfig.h"

namespace Event::services {

OResultsConfig OResultsConfig::fromVariantMap(const QVariantMap &map)
{
	OResultsConfig ret;
	ret.apiKey = map.value("apiKey").toString();
	ret.eventName = map.value("eventName").toString();
	return ret;
}

QVariantMap OResultsConfig::toVariantMap() const
{
	QVariantMap ret;
	ret["apiKey"] = apiKey;
	ret["eventName"] = eventName;
	return ret;
}

}
