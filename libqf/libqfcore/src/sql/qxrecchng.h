#pragma once

#include "../core/coreglobal.h"

#include <QString>
#include <QVariantMap>

#include <concepts>
#include <variant>

namespace qf::core::sql {

enum class RecOp {
	Insert,
	Update,
	Delete,
};

/// Record ID, either an integer or a string.
class QFCORE_DECL_EXPORT RecId
{
public:
	RecId() = default;
	template<std::integral T> requires (!std::same_as<T, bool>)
	RecId(T id) : m_id(static_cast<qint64>(id)) {}
	RecId(const QString &id) : m_id(id) {}
	RecId(const char *id) : m_id(QString::fromUtf8(id)) {}

	/// QString variant is taken as a string ID, anything else is converted to integer
	static RecId fromVariant(const QVariant &v);

	bool isInt() const { return std::holds_alternative<qint64>(m_id); }
	bool isString() const { return std::holds_alternative<QString>(m_id); }
	/// returns 0 for string ID
	qint64 toInt64() const { return isInt()? std::get<qint64>(m_id): 0; }
	QString toString() const;
	QVariant toVariant() const;

	bool operator==(const RecId &other) const = default;
private:
	std::variant<qint64, QString> m_id = qint64(0);
};

struct QFCORE_DECL_EXPORT QxRecChng
{
	QString table;
	RecId id;
	QVariantMap record;
	RecOp op = RecOp::Update;
	QString issuer;

	QVariantMap toVariantMap() const;
	static QxRecChng fromVariantMap(const QVariantMap &m);
	static QString recopToString(RecOp op);
};
}
