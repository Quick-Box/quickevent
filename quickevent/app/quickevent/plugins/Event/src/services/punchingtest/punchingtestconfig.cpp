#include "punchingtestconfig.h"

#include <QStringList>

#include <algorithm>

namespace Event::services {

PunchingTestConfig PunchingTestConfig::fromVariantMap(const QVariantMap &map)
{
	PunchingTestConfig ret;
	const QStringList ids = map.value("generatedCards").toString().split(',', Qt::SkipEmptyParts);
	for (const QString &id : ids) {
		ret.generatedCards.insert(id.toInt());
	}
	return ret;
}

QVariantMap PunchingTestConfig::toVariantMap() const
{
	QList<int> ids(generatedCards.cbegin(), generatedCards.cend());
	std::sort(ids.begin(), ids.end());
	QStringList sl;
	for (int id : ids) {
		sl << QString::number(id);
	}
	QVariantMap ret;
	ret["generatedCards"] = sl.join(',');
	return ret;
}

} // namespace Event::services
