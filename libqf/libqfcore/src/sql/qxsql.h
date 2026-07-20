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

struct DbField
{
	QString name;
};

struct QFCORE_DECL_EXPORT QueryResult
{
	QList<DbField> fields;
	QList<QList<QVariant>> rows;

	std::optional<Record> record(int i) const
	{
		if (i < 0 || i >= rows.size()) {
			return std::nullopt;
		}
		Record r;
		const auto &row = rows[i];
		for (int j = 0; j < fields.size(); ++j) {
			r[fields[j].name] = row.value(j);
		}
		return r;
	}
};

struct QFCORE_DECL_EXPORT ExecResult
{
	qint64 numRowsAffected = 0;
	std::optional<int64_t> lastInsertId = 0;
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
	void transaction(const QString &query, const QVariantList &params, QSqlDatabase db);

	qint64 createRecord(const QString &table, const Record &record, const QString &issuer);
	std::optional<Record> readRecord(const QString &table, qint64 id, const std::optional<QStringList> &fields = std::nullopt);
	bool updateRecord(const QString &table, qint64 id, const Record &record, const QString &issuer);
	bool deleteRecord(const QString &table, qint64 id, const QString &issuer);
protected:
	QList<Record> listOneOrMoreRecords(
			const QString &table,
			const std::optional<QStringList> &fields,
			const std::optional<qint64> &id,
			const std::optional<qint64> &limit);
};

class QxSqlApiImpl : public QxSqlApi
{
public:
	QxSqlApiImpl(QSqlDatabase db) : m_db(db) {}

	QueryResult query(const QString &query, const QVariantMap &params) override;
	ExecResult exec(const QString &query, const QVariantMap &params) override;
	void transaction(const QString &query, const QVariantList &params);
private:
	QSqlDatabase m_db;
};

class QFCORE_DECL_EXPORT QxSql : public QObject
{
	Q_OBJECT
public:
	QxSql(const QString &issuer, const QSqlDatabase &db = {}, QObject *parent = nullptr);
	~QxSql() override = default;

	Q_SIGNAL void recChng(const qf::core::sql::QxRecChng &recchng, QObject *source);

	QueryResult query(const QString &query, const QVariantMap &params);
	ExecResult exec(const QString &query, const QVariantMap &params);
	QList<Record> listRecords(
			const QString &table,
			const std::optional<QStringList> &fields = std::nullopt,
			const std::optional<qint64> &fromId = std::nullopt,
			const std::optional<qint64> &limit = std::nullopt);
	void transaction(const QString &query, const QVariantList &params);

	qint64 createRecord(const QString &table, const Record &record, QObject *source);
	std::optional<Record> readRecord(const QString &table, qint64 id, const std::optional<QStringList> &fields = std::nullopt);
	bool updateRecord(const QString &table, qint64 id, const Record &record, QObject *source);
	bool deleteRecord(const QString &table, qint64 id, QObject *source);

	void emitRecInserted(const QString &table, qint64 id, const QVariantMap &record, QObject *source);
	void emitRecUpdated(const QString &table, qint64 id, const QVariantMap &record, QObject *source);
	void emitRecDeleted(const QString &table, qint64 id, QObject *source);
	void emitRecChng(const qf::core::sql::QxRecChng &recchng, QObject *source);
private:
	QString m_issuer;
	QxSqlApiImpl m_sqlApi;
};

} // namespace qf::core::sql
