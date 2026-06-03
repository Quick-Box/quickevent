#include "sqlapi.h"

#include <qf/core/log.h>
#include <qf/core/sql/connection.h>
#include <qf/core/sql/query.h>
#include <qf/core/exception.h>
#include <qf/core/sql/qxrecchng.h>
#include <qf/gui/framework/application.h>

#include <shv/chainpack/rpcvalue.h>
#include <shv/coreqt/rpc.h>

#include <QCoreApplication>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlField>
#include <QVariant>
#include <algorithm>

using namespace shv::chainpack;

namespace Event::services::qx {

//==============================================
// RpcSqlField
//==============================================
RpcValue DbField::toRpcValue() const
{
	RpcValue::Map ret;
	ret["name"] = name.toStdString();
	return RpcValue(std::move(ret));
}

DbField DbField::fromRpcValue(const shv::chainpack::RpcValue &rv)
{
	DbField ret;
	const RpcValue::Map &map = rv.asMap();
	ret.name = QString::fromStdString(map.value("name").asString());
	return ret;
}

//==============================================
// ExecResult
//==============================================
RpcValue ExecResult::toRpcValue() const
{
	RpcValue::Map ret;
	ret["numRowsAffected"] = numRowsAffected;
	ret["lastInsertId"] = lastInsertId.has_value()? RpcValue(lastInsertId.value()): RpcValue(nullptr);
	return ret;
}

ExecResult ExecResult::fromRpcValue(const shv::chainpack::RpcValue &rv)
{
	ExecResult ret;
	const auto &map = rv.asMap();
	ret.numRowsAffected = map.value("numRowsAffected").toInt();
	ret.lastInsertId = map.value("lastInsertId").toInt();
	return ret;
}

//==============================================
// QueryResult
//==============================================
QVariant QueryResult::value(qsizetype row, qsizetype col) const
{
	if (row < rows.size()) {
		const auto &cells = rows[row];
		if (col < cells.size()) {
			return cells[col];
		}
	}
	return {};
}

QVariant QueryResult::value(qsizetype row, const std::string &name) const
{
	if (auto ix = columnIndex(name); ix.has_value()) {
		return value(row, ix.value());
	}
	return {};
}

void QueryResult::setValue(qsizetype row, qsizetype col, const QVariant &val)
{
	if (row < rows.size()) {
		auto &r = rows[row];
		if (col < r.size()) {
			r[col] = val;
		}
	}
}

void QueryResult::setValue(qsizetype row, const std::string &name, const QVariant &val)
{
	if (auto ix = columnIndex(name); ix.has_value()) {
		setValue(row, ix.value(), val);
	}
}

RpcValue::List QueryResult::toRecordList() const
{
	RpcValue::List ret;
	for (const auto &row : rows) {
		RpcValue::Map rec;
		int n = 0;
		for (const auto &field : fields) {
			rec[field.name.toStdString()] = shv::coreqt::rpc::qVariantToRpcValue(row.value(n++));
		}
		ret.push_back(rec);
	}
	return ret;
}

std::optional<qsizetype> QueryResult::columnIndex(const std::string &name) const
{
	for (size_t col = 0; col < fields.size(); ++col) {
		const auto &fld = fields[col];
		if (fld.name.toStdString() == name) {
			return col;
		}
	}
	return {};
}

RpcValue QueryResult::toRpcValue() const
{
	RpcValue::Map ret;
	RpcValue::List flds;
	for(const auto &fld : this->fields)
		flds.push_back(fld.toRpcValue());
	RpcValue::List rpc_rows;
	for (const auto &row : rows) {
		RpcValue::List rpc_row;
		for (const auto &cell : row) {
			rpc_row.push_back(shv::coreqt::rpc::qVariantToRpcValue(cell));
		}
		rpc_rows.push_back(std::move(rpc_row));
	}
	ret["fields"] = flds;
	ret["rows"] = std::move(rpc_rows);
	return ret;
}

QueryResult QueryResult::fromRpcValue(const RpcValue &rv)
{
	QueryResult ret;
	const auto &map = rv.asMap();
	const auto &flds = map.valref("fields").asList();
	for(const auto &fv : flds) {
		ret.fields.push_back(DbField::fromRpcValue(fv));
	}
	for (const auto &rpc_row : map.value("rows").asList()) {
		QueryResult::Row row;
		for (const auto &cell : rpc_row.asList()) {
			row.push_back(shv::coreqt::rpc::rpcValueToQVariant(cell));
		}
		ret.rows.push_back(row);
	}
	return ret;
}

SqlQueryAndParams SqlQueryAndParams::fromRpcValue(const shv::chainpack::RpcValue &rv)
{
	const auto &lst = rv.asList();
	auto sql_query = QString::fromStdString(lst.valref(0).asString());
	const auto &sql_params = lst.valref(1);
	return SqlQueryAndParams { .query = sql_query, .params = shv::coreqt::rpc::rpcValueToQVariant(sql_params).toMap() };
}


RpcValue qxRecChngToRpcValue(const qf::core::sql::QxRecChng &chng)
{
	auto m = chng.toVariantMap();
	return shv::coreqt::rpc::qVariantToRpcValue(m);
}

//==============================================
// SqlApi
//==============================================
SqlApi::SqlApi(QObject *parent)
	: QObject{parent}
{
	// connect(qf::gui::framework::Application::instance(), &qf::gui::framework::Application::qxRecChng, )
}

// void SqlApi::emitRecChng(const qf::core::sql::QxRecChng &chng)
// {
// 	qfInfo() << "REC_CHNG:" << qxRecChngToRpcValue(chng).toCpon();
// 	emit recchng(chng);
// }

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

void bindParams(qf::core::sql::Query &q, const Record &params)
{
	for (const auto &[k, v] : params.asKeyValueRange()) {
		q.bindValue(':' + k, v);
	}
}

QueryResult sqlQuery(const SqlQueryAndParams &params)
{
	qf::core::sql::Query q;
	q.prepare(params.query, qf::core::Exception::Throw);
	bindParams(q, params.params);
	q.exec(qf::core::Exception::Throw);

	QueryResult ret;
	QSqlRecord rec = q.record();
	for (int i = 0; i < rec.count(); ++i) {
		QSqlField fld = rec.field(i);
		DbField rfld;
		rfld.name = fld.name();
		// rfld.name.replace("__", ".");
		ret.fields.push_back(rfld);
	}
	while(q.next()) {
		QueryResult::Row row;
		for (int i = 0; i < rec.count(); ++i) {
			row.push_back(q.value(i));
			//shvError() << v << v.isNull() << jsv.toVariant() << jsv.toVariant().isNull();
		}
		ret.rows.insert(ret.rows.size(), row);
	}
	return ret;
}

ExecResult sqlExec(const SqlQueryAndParams &params)
{
	qf::core::sql::Query q;
	q.prepare(params.query, qf::core::Exception::Throw);
	bindParams(q, params.params);
	q.exec(qf::core::Exception::Throw);

	ExecResult ret;
	ret.numRowsAffected = q.numRowsAffected();
	ret.lastInsertId = q.lastInsertId().toInt();
	return ret;
}

}

ExecResult SqlApi::exec(const SqlQueryAndParams &params)
{
	return sqlExec(params);
}

QueryResult SqlApi::query(const SqlQueryAndParams &params)
{
	return sqlQuery(params);
}

void SqlApi::transaction(const std::string &query, const shv::chainpack::RpcValue::List &params)
{
	auto conn = qf::core::sql::Connection::forName();
	Transaction tranaction(conn);
	qf::core::sql::Query q(conn);
	q.prepare(QString::fromUtf8(query), qf::core::Exception::Throw);
	for (const auto &param : params) {
		for (const auto &[k, v] : param.asMap()) {
			bool ok;
			QVariant val = shv::coreqt::rpc::rpcValueToQVariant(v, &ok);
			if (!ok) {
				QF_EXCEPTION(QStringLiteral("Cannot convert SHV type: %1 to QVariant").arg(v.typeName()));
			}
			q.bindValue(':' + QString::fromStdString(k), val);
		}
		q.exec(qf::core::Exception::Throw);
	}
	tranaction.commit();
}

QueryResult SqlApi::list(const std::string &table, const std::vector<std::string> &fields, std::optional<int64_t> ids_above, std::optional<int64_t> limit)
{
	QStringList qfields;
	for (const auto &fn : fields) {
		qfields << QString::fromStdString(fn);
	}
	if (qfields.isEmpty()) {
		qfields << "*";
	}
	QString sql_query = QStringLiteral("SELECT %1 FROM %2").arg(qfields.join(',')).arg(QString::fromStdString(table));
	if (ids_above.has_value()) {
		sql_query += " WHERE id > " + QString::number(ids_above.value());
	}
	if (limit.has_value()) {
		sql_query += " LIMIT " + QString::number(limit.value());
	}
	auto res = sqlQuery(SqlQueryAndParams { .query = sql_query, .params = {}});
	return res;
}
namespace {
std::string to_lower(const std::string &s)
{
	std::string result;
	result.reserve(s.size());

	std::ranges::transform(s, std::back_inserter(result),
			[](unsigned char c) { return std::tolower(c); });

	return result;
}

[[maybe_unused]] Record normalizeFieldNames(const Record &rec)
{
	Record ret;
	for (const auto &[k, v] : rec.asKeyValueRange()) {
		ret[QString::fromStdString(to_lower(k.toStdString()))] = v;
	}
	return ret;
}
}
int64_t SqlApi::create(const std::string &table, const RpcValue::Map &record)
{
	QStringList fields;
	QStringList placeholders;
	for (const auto &[k, v] : record) {
		Q_UNUSED(v)
		auto qk = QString::fromStdString(k);
		fields << qk;
		placeholders << ':' + qk;
	}
	QString sql_query = QStringLiteral("INSERT INTO %1 (%2) VALUES (%3)")
			.arg(QString::fromStdString(table))
			.arg(fields.join(','))
			.arg(placeholders.join(','));
	qf::core::sql::Query q;
	q.prepare(sql_query, qf::core::Exception::Throw);
	for (const auto &[k, v] : record) {
		q.bindValue(':' + QString::fromStdString(k), shv::coreqt::rpc::rpcValueToQVariant(v));
	}
	q.exec(qf::core::Exception::Throw);
	auto id = q.lastInsertId().toInt();
	// SqlApi::instance()->emitRecChng(qf::core::sql::QxRecChng {
	// 	.table = QString::fromStdString(table),
	// 	.id = id,
	// 	.record = shv::coreqt::rpc::rpcValueToQVariant(normalizeFieldNames(record)).toMap(),
	// 	.op = qf::core::sql::RecOp::Insert,
	// 	.issuer = {}
	// });
	return id;
}

std::optional<Record> SqlApi::read(const std::string &table, int64_t id, const std::vector<std::string> &fields)
{
	QStringList qfields;
	for (const auto &fn : fields) {
		qfields << QString::fromStdString(fn);
	}
	if (qfields.isEmpty()) {
		qfields << "*";
	}
	QString sql_query = QStringLiteral("SELECT %1 FROM %2 WHERE id = %3")
			.arg(qfields.join(','))
			.arg(QString::fromStdString(table))
			.arg(id) ;
	auto res = sqlQuery(SqlQueryAndParams { .query = sql_query, .params = {}});
	if (res.rows.empty()) {
		return {};
	}
	Record ret;
	const auto &row = res.rows.first();
	for (qsizetype i = 0; i < std::ssize(res.fields); ++i) {
		ret[res.fields[i].name] = row.value(i);
	}
	return ret;
}

bool SqlApi::update(const std::string &table, int64_t id, const RpcValue::Map &record)
{
	QStringList fields;
	for (const auto &[k, v] : record) {
		Q_UNUSED(v)
		auto qk = QString::fromStdString(k);
		fields << qk + " = :" + qk;
	}
	QString sql_query = QStringLiteral("UPDATE %1 SET %2 WHERE id = %3")
			.arg(QString::fromStdString(table))
			.arg(fields.join(','))
			.arg(id);
	qf::core::sql::Query q;
	q.prepare(sql_query, qf::core::Exception::Throw);
	for (const auto &[k, v] : record) {
		q.bindValue(':' + QString::fromStdString(k), shv::coreqt::rpc::rpcValueToQVariant(v));
	}
	q.exec(qf::core::Exception::Throw);
	bool updated = q.numRowsAffected() == 1;
	if (updated) {
		// SqlApi::instance()->emitRecChng(qf::core::sql::QxRecChng {
		// 	.table = QString::fromStdString(table),
		// 	.id = id,
		// 	.record = shv::coreqt::rpc::rpcValueToQVariant(normalizeFieldNames(record)).toMap(),
		// 	.op = qf::core::sql::RecOp::Update,
		// 	.issuer = {}
		// });
	}
	return updated;
}

bool SqlApi::drop(const std::string &table, int64_t id)
{
	QString sql_query = QStringLiteral("DELETE FROM %1 WHERE id = %2")
			.arg(QString::fromStdString(table))
			.arg(id);
	qf::core::sql::Query q;
	q.exec(sql_query, qf::core::Exception::Throw);
	bool is_drop = q.numRowsAffected() == 1;
	if (is_drop) {
		// SqlApi::instance()->emitRecChng(qf::core::sql::QxRecChng {
		// 	.table = QString::fromStdString(table),
		// 	.id = id,
		// 	.record = {},
		// 	.op = qf::core::sql::RecOp::Delete,
		// 	.issuer = {}
		// });
	}
	return is_drop;
}

}
