#include "qxrecchng.h"

#include "../core/log.h"

namespace qf::core::sql {
namespace {
RecOp recopFromString(const QString &s)
{
	if (s == "Insert") return RecOp::Insert;
	if (s == "Delete") return RecOp::Delete;
	if (s == "Update") return RecOp::Update;
	qfWarning() << "Undefined record operation:" << s;
	return RecOp::Update;
}
}
RecId RecId::fromVariant(const QVariant &v)
{
	if (v.typeId() == QMetaType::QString) {
		return RecId(v.toString());
	}
	return RecId(v.toLongLong());
}

QString RecId::toString() const
{
	if (isString()) {
		return std::get<QString>(m_id);
	}
	return QString::number(std::get<qint64>(m_id));
}

QVariant RecId::toVariant() const
{
	if (isString()) {
		return std::get<QString>(m_id);
	}
	return std::get<qint64>(m_id);
}

QVariantMap QxRecChng::toVariantMap() const
{
	QVariantMap ret;
	ret["table"] = table;
	ret["op"] = recopToString(op);
	ret["id"] = id.toVariant();
	if (op != RecOp::Delete) {
		ret["record"] = record;
	}
	if (!issuer.isEmpty()) {
		ret["issuer"] = issuer;
	}
	return ret;
}

QxRecChng QxRecChng::fromVariantMap(const QVariantMap &m)
{
	QxRecChng ret;
	ret.table = m.value("table").toString();
	ret.op = recopFromString(m.value("op").toString());
	ret.id = RecId::fromVariant(m.value("id"));
	if (ret.op != RecOp::Delete) {
		ret.record = m.value("record").toMap();
	}
	ret.issuer = m.value("issuer").toString();
	return ret;
}

QString QxRecChng::recopToString(RecOp op)
{
	switch (op) {
	case RecOp::Insert: return "Insert";
	case RecOp::Update: return "Update";
	case RecOp::Delete: return "Delete";
	}
	return QString();
}

}
