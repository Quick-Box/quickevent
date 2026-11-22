#pragma once

#include <QSet>
#include <QVariantMap>

namespace Event::services {

struct PunchingTestConfig
{
	static PunchingTestConfig fromVariantMap(const QVariantMap &map);
	QVariantMap toVariantMap() const;

	// Ids of the competitors a card has been generated for. Kept, because a card with an unknown
	// or duplicate SI stays unassigned and its competitor would be generated again otherwise.
	QSet<int> generatedCards;
};

} // namespace Event::services
