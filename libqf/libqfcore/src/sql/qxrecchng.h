#pragma once

#include "../core/coreglobal.h"

#include <QString>
#include <QVariantMap>

namespace qf::core::sql {

enum class RecOp {
	Insert,
	Update,
	Delete,
};

struct QFCORE_DECL_EXPORT QxRecChng
{
	QString table;
	qint64 id = 0;
	QVariantMap record;
	RecOp op = RecOp::Update;
	QString issuer;

	QVariantMap toVariantMap() const;
	static QxRecChng fromVariantMap(const QVariantMap &m);
	static QString recopToString(RecOp op);
};
}
