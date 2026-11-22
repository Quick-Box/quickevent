#ifndef QUICKEVENT_CORE_UTILS_H
#define QUICKEVENT_CORE_UTILS_H

#include "quickeventcoreglobal.h"

class QDateTime;

namespace quickevent {
namespace core {

class QUICKEVENTCORE_DECL_EXPORT Utils
{
public:
	static QString dateTimeToIsoStringWithUtcOffset(QDateTime dt);
};

/// Position in a list sorted by value, equal values share the position (1, 2, 2, 4).
/// Pass times quantized to event precision, so that competitors with the same displayed time get the same position.
class SharedPosition
{
public:
	int next(int position, int value)
	{
		if(value != m_prevValue) {
			m_position = position;
			m_prevValue = value;
		}
		return m_position;
	}
private:
	int m_position = 0;
	int m_prevValue = -1;
};

} // namespace core
} // namespace quickevent

#endif // QUICKEVENT_CORE_UTILS_H
