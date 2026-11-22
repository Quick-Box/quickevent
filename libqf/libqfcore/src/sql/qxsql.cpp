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

QList<Record> QxSqlApi::listRecords(const QString &table, const std::optional<QStringList> &fields, const std::optional<RecId> &ids_above, const std::optional<qint64> &limit)
{
	std::optional<IdFilter> id_filter;
	if (ids_above.has_value()) {
		id_filter = IdFilter{.op = IdFilter::Op::Above, .id = *ids_above};
	}
	return listOneOrMoreRecords(table, fields, id_filter, limit);
}

RecId QxSqlApi::createRecord(const QString &table, const Record &record, const QString &issuer)
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

	const auto &id = result.rows[0][0];
	if (id.typeId() == QMetaType::QString) {
		return RecId(id.toString());
	}
	bool ok = false;
	qint64 int_id = id.toLongLong(&ok);
	if (!ok) {
		throw qf::core::Exception(QStringLiteral("Insert should return an integer or string ID, got: %1").arg(id.typeName()));
	}
	return RecId(int_id);
}

std::optional<Record> QxSqlApi::readRecord(const QString &table, const RecId &id, const std::optional<QStringList> &fields)
{
	auto records = listOneOrMoreRecords(table, fields, IdFilter{.op = IdFilter::Op::Equal, .id = id}, 1);
	if (!records.isEmpty()) {
		return records.first();
	}
	return std::nullopt;
}

bool QxSqlApi::updateRecord(const QString &table, const RecId &id, const Record &record, const QString &issuer)
{
	Q_UNUSED(issuer)
	static const auto ID = QStringLiteral("id");
	if (auto it = record.constFind(ID); it != record.cend() && RecId::fromVariant(it.value()) != id) {
		throw qf::core::Exception("Record ID cannot be updated");
	}
	QStringList keyVals;
	for (auto it = record.begin(); it != record.end(); ++it) {
		if (it.key() != ID) {
			keyVals << QString("%1 = :%1").arg(it.key());
		}
	}
	if (keyVals.isEmpty()) {
		throw qf::core::Exception("Nothing to update");
	}

	QString qs = QString("UPDATE %1 SET %2 WHERE id = :id").arg(table, keyVals.join(", "));
	auto params = record;
	params[ID] = id.toVariant();
	ExecResult res = exec(qs, params);
	return res.numRowsAffected == 1;
}

bool QxSqlApi::deleteRecord(const QString &table, const RecId &id, const QString &issuer)
{
	Q_UNUSED(issuer)
	QString qs = QString("DELETE FROM %1 WHERE id = :id").arg(table);

	ExecResult res = exec(qs, {{"id", id.toVariant()}});
	return res.numRowsAffected == 1;
}

QList<Record> QxSqlApi::listOneOrMoreRecords(const QString &table, const std::optional<QStringList> &fields, const std::optional<IdFilter> &id_filter, const std::optional<qint64> &limit)
{
	QString fieldsStr = (fields && !fields->isEmpty()) ? fields->join(", ") : "*";

	QString qs = QString("SELECT %1 FROM %2").arg(fieldsStr, table);

	Record params;
	if (id_filter.has_value()) {
		auto op = id_filter->op == IdFilter::Op::Equal? QStringLiteral("="): QStringLiteral(">");
		qs += QString(" WHERE id %1 :id").arg(op);
		params["id"] = id_filter->id.toVariant();
	}

	qs += QString(" ORDER BY id");

	if (limit.has_value()) {
		qs += QString(" LIMIT %1").arg(*limit);
	}

	QueryResult result = query(qs, params);

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
			auto v = q.value(i);
			if (v.isNull()) {
				v = {};
			}
			row.append(v);
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
	// QPSQL lastInsertId() runs SELECT lastval(), it fails when no sequence was used in the session yet and aborts the open transaction
	if (!query.trimmed().startsWith(QLatin1String("INSERT"), Qt::CaseInsensitive)) {
		return result;
	}
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

QList<Record> QxSql::listRecords(const QString &table, const std::optional<QStringList> &fields, const std::optional<RecId> &ids_above, const std::optional<qint64> &limit)
{
	return m_sqlApi.listRecords(table, fields, ids_above, limit);
}

void QxSql::transaction(const QString &query, const QVariantList &params)
{
	m_sqlApi.transaction(query, params);
}

RecId QxSql::createRecord(const QString &table, const Record &record, QObject *source)
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

std::optional<Record> QxSql::readRecord(const QString &table, const RecId &id, const std::optional<QStringList> &fields)
{
	return m_sqlApi.readRecord(table, id, fields);
}

bool QxSql::updateRecord(const QString &table, const RecId &id, const Record &record, QObject *source)
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

bool QxSql::deleteRecord(const QString &table, const RecId &id, QObject *source)
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

void QxSql::emitRecInserted(const QString &table, const RecId &id, const QVariantMap &record, QObject *source)
{
	emit recChng(qf::core::sql::QxRecChng{
		.table = table,
		.id = id,
		.record = record,
		.op = qf::core::sql::RecOp::Insert,
		.issuer = m_issuer
	}, source);
}

void QxSql::emitRecUpdated(const QString &table, const RecId &id, const QVariantMap &record, QObject *source)
{
	emit recChng(qf::core::sql::QxRecChng{
		.table = table,
		.id = id,
		.record = record,
		.op = qf::core::sql::RecOp::Update,
		.issuer = m_issuer
	}, source);
}

void QxSql::emitRecDeleted(const QString &table, const RecId &id, QObject *source)
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
