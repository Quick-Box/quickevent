#include "radiosenderconfig.h"

namespace Event::services {

RadioSenderConfig RadioSenderConfig::fromVariantMap(const QVariantMap &map)
{
	RadioSenderConfig config;
	config.listenAddress = map.value(QStringLiteral("listenAddress"),
		map.value(QStringLiteral("host"), config.listenAddress)).toString();
	config.port = map.value(QStringLiteral("port"), config.port).toInt();
	config.startControl = map.value(QStringLiteral("startControl"), config.startControl).toInt();
	config.finishControl = map.value(QStringLiteral("finishControl"), config.finishControl).toInt();
	return config;
}

QVariantMap RadioSenderConfig::toVariantMap() const
{
	return {
		{QStringLiteral("listenAddress"), listenAddress},
		{QStringLiteral("port"), port},
		{QStringLiteral("startControl"), startControl},
		{QStringLiteral("finishControl"), finishControl},
	};
}

} // namespace Event::services
