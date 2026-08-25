#pragma once

#include <QString>
#include <QVariantMap>

namespace Event::services {

struct RadioSenderConfig
{
	static RadioSenderConfig fromVariantMap(const QVariantMap &map);
	QVariantMap toVariantMap() const;

	QString listenAddress = QStringLiteral("0.0.0.0");
	int port = 1122;
	int startControl = 11;
	int finishControl = 901;
};

} // namespace Event::services
