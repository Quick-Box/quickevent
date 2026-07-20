#include "qxsql.h"

#include <qf/core/exception.h>
#include <qf/core/log.h>
#include <qf/core/sql/query.h>

#include <QSqlQuery>
#include <QSqlError>
#include <QSqlRecord>

namespace qf::core::sql {

//================================================================
// QxSqlApi
//================================================================
namespace {

class Transaction
{
public:
	Transaction(QSqlDatabase db) : m_db(db) {
		if (!m_db.transaction()) {
			qfWarning() << "BEGIN transaction error:" << m_db.lastError().text();
			throw std::runtime_error("BEGIN transaction error");
		}
	}
	~Transaction() {
		if (m_inTransaction) {
			m_db.rollback();
		}
	}
	void commit() {
		if (!m_db.commit()) {
			qfWarning() << "COMMIT transaction error:" << m_db.lastError().text();
			throw std::runtime_error("COMMIT transaction error");
		}
		m_inTransaction = false;
	}
private:
	QSqlDatabase m_db;
	bool m_inTransaction = true;
};
}
void QxSqlApi::transaction(const QString &query, const QVariantList &params, QSqlDatabase db)
{
	Transaction transaction(db);
	qf::core::sql::Query q(db);
	q.prepare(query, qf::core::Exception::Throw);
	for (const auto &param : params) {
		auto m = param.toMap();
		for (const auto &[k, v] : m.asKeyValueRange()) {
			q.bindValue(':' + k, v);
		}
		q.exec(qf::core::Exception::Throw);
	}
	transaction.commit();
}

qint64 QxSqlApi::createRecord(const QString &table, const Record &record, const QString &issuer)
{
	Q_UNUSED(issuer)
	QStringList keys;
	QStringList vals;

	for (auto it = record.begin(); it != record.end(); ++it) {
		keys << it.key();
		vals << (":" + it.key());
	}

	QString qs = QString("INSERT INTO %1 (%2) VALUES (%3) RETURNING id")
			.arg(table, keys.join(", "), vals.join(", "));

	QueryResult result = query(qs, record);

	if (result.rows.isEmpty() || result.rows[0].isEmpty()) {
		throw qf::core::Exception("Insert should return an ID");
	}

	bool ok = false;
	qint64 id = result.rows[0][0].toLongLong(&ok);

	if (!ok) {
		throw qf::core::Exception("Insert should return an integer ID");
	}

	return id;
}

std::optional<Record> QxSqlApi::readRecord(const QString &table, qint64 id, const std::optional<QStringList> &fields)
{
	auto records = listOneOrMoreRecords(table, fields, id, 1);
	if (!records.isEmpty()) {
		return records.first();
	}
	return std::nullopt;
}

bool QxSqlApi::updateRecord(const QString &table, qint64 id, const Record &record, const QString &issuer)
{
	Q_UNUSED(issuer)
	QStringList keyVals;

	for (auto it = record.begin(); it != record.end(); ++it) {
		keyVals << QString("%1 = :%1").arg(it.key());
	}

	QString qs = QString("UPDATE %1 SET %2 WHERE id = %3")
			.arg(table, keyVals.join(", "), QString::number(id));

	ExecResult res = exec(qs, record);
	return res.numRowsAffected == 1;
}

bool QxSqlApi::deleteRecord(const QString &table, qint64 id, const QString &issuer)
{
	Q_UNUSED(issuer)
	QString qs = QString("DELETE FROM %1 WHERE id = %2").arg(table, QString::number(id));

	ExecResult res = exec(qs, {});
	return res.numRowsAffected == 1;
}

QList<Record> QxSqlApi::listOneOrMoreRecords(const QString &table, const std::optional<QStringList> &fields, const std::optional<qint64> &id, const std::optional<qint64> &limit)
{
	QString fieldsStr =
			(fields && !fields->isEmpty()) ? fields->join(", ") : "*";

	QString qs = QString("SELECT %1 FROM %2").arg(fieldsStr, table);

	if (id.has_value()) {
		qs += QString(" WHERE id >= %1").arg(*id);
	}

	if (limit.has_value()) {
		qs += QString(" LIMIT %1").arg(*limit);
	}

	QueryResult result = query(qs, {});

	QList<Record> records;
	records.reserve(result.rows.size()); // still valid for QList (Qt 6)

	for (int i = 0; i < result.rows.size(); ++i) {
		auto rec = result.record(i);
		if (rec.has_value()) {
			records.append(*rec);
		}
	}

	return records;
}

//================================================================
// QxSqlApiImpl
//================================================================
QueryResult QxSqlApiImpl::query(const QString &query, const QVariantMap &params)
{
	// qDebug() << query << params;
	QSqlQuery q(m_db);
	if (!q.prepare(query)) {
		throw qf::core::Exception(q.lastError().text());
	}
	for (const auto &[key, val] : params.asKeyValueRange()) {
		q.bindValue(QStringLiteral(":%1").arg(key), val);
	}
	if (!q.exec()) {
		throw qf::core::Exception(q.lastError().text());
	}
	QueryResult result;
	for (int i = 0; i < q.record().count(); ++i) {
		result.fields.append(DbField{.name = q.record().fieldName(i).toLower()});
	}
	while (q.next()) {
		QList<QVariant> row;
		for (int i = 0; i < q.record().count(); ++i) {
			row.append(q.value(i));
		}
		result.rows.insert(result.rows.size(), row);
	}
	return result;
}

ExecResult QxSqlApiImpl::exec(const QString &query, const QVariantMap &params)
{
	QSqlQuery q(m_db);
	if (!q.prepare(query)) {
		throw qf::core::Exception(q.lastError().text());
	}
	for (const auto &[key, val] : params.asKeyValueRange()) {
		q.bindValue(QStringLiteral(":%1").arg(key), val);
	}
	if (!q.exec()) {
		throw qf::core::Exception(q.lastError().text());
	}
	ExecResult result;
	result.numRowsAffected = q.numRowsAffected();
	if (auto id = q.lastInsertId().toLongLong(); id > 0) {
		result.lastInsertId = id;
	}
	return result;
}

void QxSqlApiImpl::transaction(const QString &query, const QVariantList &params)
{
	QxSqlApi::transaction(query, params, m_db);
}

//================================================================
// QxSql
//================================================================
QxSql::QxSql(const QString &issuer, const QSqlDatabase &db, QObject *parent)
	: QObject(parent)
	, m_issuer(issuer)
	, m_sqlApi(db)
{
}

QueryResult QxSql::query(const QString &query, const QVariantMap &params)
{
	return m_sqlApi.query(query, params);
}

ExecResult QxSql::exec(const QString &query, const QVariantMap &params)
{
	return m_sqlApi.exec(query, params);
}

QList<Record> QxSql::listRecords(const QString &table, const std::optional<QStringList> &fields, const std::optional<qint64> &fromId, const std::optional<qint64> &limit)
{
	return m_sqlApi.listRecords(table, fields, fromId, limit);
}

void QxSql::transaction(const QString &query, const QVariantList &params)
{
	m_sqlApi.transaction(query, params);
}

qint64 QxSql::createRecord(const QString &table, const Record &record, QObject *source)
{
	auto id = m_sqlApi.createRecord(table, record, m_issuer);
	emit recChng(qf::core::sql::QxRecChng{.table = table,
										  .id = id,
										  .record = record,
										  .op = qf::core::sql::RecOp::Insert,
										  .issuer = m_issuer},
				 source);
	return id;
}

std::optional<Record> QxSql::readRecord(const QString &table, qint64 id, const std::optional<QStringList> &fields)
{
	return m_sqlApi.readRecord(table, id, fields);
}

bool QxSql::updateRecord(const QString &table, qint64 id, const Record &record, QObject *source)
{
	auto ok = m_sqlApi.updateRecord(table, id, record, m_issuer);
	if (ok) {
		emit recChng(qf::core::sql::QxRecChng{.table = table,
											  .id = id,
											  .record = record,
											  .op = qf::core::sql::RecOp::Update,
											  .issuer = m_issuer},
					 source);
	}
	return ok;
}

bool QxSql::deleteRecord(const QString &table, qint64 id, QObject *source)
{
	auto ok = m_sqlApi.deleteRecord(table, id, m_issuer);
	if (ok) {
		emit recChng(qf::core::sql::QxRecChng{.table = table,
											  .id = id,
											  .record = {},
											  .op = qf::core::sql::RecOp::Delete,
											  .issuer = m_issuer},
					 source);
	}
	return ok;
}

void QxSql::emitRecInserted(const QString &table, qint64 id, const QVariantMap &record, QObject *source)
{
	emit recChng(qf::core::sql::QxRecChng{
		.table = table,
		.id = id,
		.record = record,
		.op = qf::core::sql::RecOp::Insert,
		.issuer = m_issuer
	}, source);
}

void QxSql::emitRecUpdated(const QString &table, qint64 id, const QVariantMap &record, QObject *source)
{
	emit recChng(qf::core::sql::QxRecChng{
		.table = table,
		.id = id,
		.record = record,
		.op = qf::core::sql::RecOp::Update,
		.issuer = m_issuer
	}, source);
}

void QxSql::emitRecDeleted(const QString &table, qint64 id, QObject *source)
{
	emit recChng(qf::core::sql::QxRecChng{
		.table = table,
		.id = id,
		.record = {},
		.op = qf::core::sql::RecOp::Delete,
		.issuer = m_issuer
	}, source);
}

void QxSql::emitRecChng(const core::sql::QxRecChng &recchng, QObject *source)
{
	emit recChng(recchng, source);
}

}
