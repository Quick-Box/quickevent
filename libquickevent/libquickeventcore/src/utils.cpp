#include "utils.h"

#include <QDateTime>
#include <QTimeZone>


namespace quickevent::core {

 QString Utils::dateTimeToIsoStringWithUtcOffset(QDateTime dt)
{
#if QT_VERSION < QT_VERSION_CHECK(6, 8, 0)
	// offset is missing if not explicitly set, see https://bugreports.qt.io/browse/QTBUG-26161?focusedCommentId=554227
	dt.setOffsetFromUtc(dt.offsetFromUtc());
#else
	dt.setTimeZone(dt.timeZone());
#endif
	return dt.toString(Qt::ISODateWithMs);
}

} // namespace quickevent::core

