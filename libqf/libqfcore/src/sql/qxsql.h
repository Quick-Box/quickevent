#pragma once

#include "qxrecchng.h"

#include <QList>
#include <QMap>
#include <QString>
#include <QStringList>
#include <QVariant>
#include <QSqlDatabase>

#include <optional>

namespace qf::core::sql {

using Record = QVariantMap;

struct QFCORE_DECL_EXPORT QueryResult
{
	QStringList columns;
	QList<QList<QVariant>> rows;

	std::optional<Record> record(int i) const
	{
		if (i < 0 || i >= rows.size()) {
			return std::nullopt;
		}
		Record r;
		const auto &row = rows[i];
		for (int j = 0; j < columns.size(); ++j) {
			r[columns[j]] = row.value(j);
		}
		return r;
	}
};

struct QFCORE_DECL_EXPORT ExecResult
{
	qint64 rowsAffected = 0;
};

class QFCORE_DECL_EXPORT QxSqlApi
{
public:
	virtual ~QxSqlApi() = default;
	virtual QueryResult query(const QString &query, const QVariantMap &params) = 0;
	virtual ExecResult exec(const QString &query, const QVariantMap &params) = 0;

	QList<Record> listRecords(
			const QString &table,
			const std::optional<QStringList> &fields = std::nullopt,
			const std::optional<qint64> &fromId = std::nullopt,
			const std::optional<qint64> &limit = std::nullopt)
	{
		return listOneOrMoreRecords(table, fields, fromId, limit);
	}

	virtual qint64 createRecord(const QString &table, const Record &record, const QString &issuer);
	virtual std::optional<Record> readRecord(const QString &table, qint64 id, const std::optional<QStringList> &fields = std::nullopt);
	virtual bool updateRecord(const QString &table, qint64 id, const Record &record, const QString &issuer);
	virtual bool deleteRecord(const QString &table, qint64 id, const QString &issuer);
protected:
	QList<Record> listOneOrMoreRecords(
			const QString &table,
			const std::optional<QStringList> &fields,
			const std::optional<qint64> &id,
			const std::optional<qint64> &limit);
};

class QFCORE_DECL_EXPORT QxSql : public QObject, public QxSqlApi
{
	Q_OBJECT
public:
	QxSql(const QSqlDatabase &db = QSqlDatabase());
	~QxSql() override = default;

	Q_SIGNAL void dbRecChng(const qf::core::sql::QxRecChng &recchng);

	QueryResult query(const QString &query, const QVariantMap &params) override;
	ExecResult exec(const QString &query, const QVariantMap &params) override;

	qint64 createRecord(const QString &table, const Record &record, const QString &issuer) override;
	bool updateRecord(const QString &table, qint64 id, const Record &record, const QString &issuer) override;
	bool deleteRecord(const QString &table, qint64 id, const QString &issuer) override;
private:
	QSqlDatabase m_db;
};

} // namespace qf::core::sql
