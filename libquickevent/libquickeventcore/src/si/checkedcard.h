#pragma once

#include "../quickeventcoreglobal.h"
#include "checkedpunch.h"

#include <qf/core/utils.h>

#include <QVariantMap>

namespace quickevent {
namespace core {
namespace si {

class QUICKEVENTCORE_DECL_EXPORT CheckedCard : public QVariantMap
{
private:
	typedef QVariantMap Super;

	QF_VARIANTMAP_FIELD(int, r, setR, unId)
	QF_VARIANTMAP_FIELD(int, c, setC, ourseId)
	QF_VARIANTMAP_FIELD(int, s, setS, tageStartTimeMs) //< stage start till midnight
	QF_VARIANTMAP_FIELD(int, c, setC, heckTimeMs) //< check time till stage start
	QF_VARIANTMAP_FIELD(int, s, setS, tartTimeMs) //< start time till stage start
	QF_VARIANTMAP_FIELD(int, f, setF, inishTimeMs) //< finish time till stage start
	QF_VARIANTMAP_FIELD(int, c, setC, ardNumber)
	// punch times for course including finish, all measured till competitor start time
	// this list contains all the course puches for correct punching
	// if runner has punched different control, it is not present in this list
	// if runner has missed some of his control, the empty place is here
	QF_VARIANTMAP_FIELD(QVariantList, p, setP, unches)
	QF_VARIANTMAP_FIELD(bool, is, set, BadCheck)
	QF_VARIANTMAP_FIELD(bool, is, set, MisPunch)
	QF_VARIANTMAP_FIELD(QVariantMap, d, setD, ata)
	QF_VARIANTMAP_FIELD(QVariantList, m, setM, issingCodes)
public:
	CheckedCard(const QVariantMap &data = QVariantMap());

	int punchCount() const;
	CheckedPunch punchAt(int i) const;

	int timeMs() const;
	QString toString() const;

	bool isOk() const {return !isBadCheck() && !isMisPunch();}
};

}}}

