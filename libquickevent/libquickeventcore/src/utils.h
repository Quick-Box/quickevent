#ifndef QUICKEVENT_CORE_UTILS_H
#define QUICKEVENT_CORE_UTILS_H

#include "quickeventcoreglobal.h"

#include <optional>

class QDateTime;

namespace quickevent {
namespace core {

class QUICKEVENTCORE_DECL_EXPORT Utils
{
public:
	static QString dateTimeToIsoStringWithUtcOffset(QDateTime dt);
};

/// Position in a list sorted by time, equal times share the position (1, 2, 2, 4).
/// Pass times quantized to event precision, so that competitors with the same displayed time get the same position.
class SharedPosition
{
public:
	int next(int position, int time)
	{
		if(time != m_prevTime) {
			m_position = position;
			m_prevTime = time;
		}
		return m_position;
	}
private:
	int m_position = 0;
	std::optional<int> m_prevTime;
};

} // namespace core
} // namespace quickevent

#endif // QUICKEVENT_CORE_UTILS_H
