#pragma once

#include <QVariantMap>
#include <QString>

namespace Event::services {

struct OResultsConfig
{
	static OResultsConfig fromVariantMap(const QVariantMap &map);
	QVariantMap toVariantMap() const;

	QString apiKey;
	QString eventName;
};

} // namespace Event::services
