#pragma once

#include "../core/coreglobal.h"

namespace qf::core::sql {

enum class QxRecOp { Insert, Update, Delete, };

struct QFCORE_DECL_EXPORT QxRecChng
{
	QString table;
	int64_t id;
	QVariant record;
	QxRecOp op;
};

}
