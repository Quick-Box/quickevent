#include "sqlapi.h"

#include <qf/core/log.h>
#include <qf/core/sql/connection.h>
#include <qf/core/sql/query.h>
#include <qf/core/exception.h>

#include <shv/chainpack/rpcvalue.h>
#include <shv/coreqt/rpc.h>

#include <QCoreApplication>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlField>
#include <QVariant>

using namespace shv::chainpack;

namespace Event::services::qx {

//==============================================
// RpcSqlField
//==============================================
RpcValue RpcSqlField::toRpcValue() const
{
	RpcValue::Map ret;
	ret["name"] = name;
	return RpcValue(std::move(ret));
}

RpcSqlField RpcSqlField::fromRpcValue(const shv::chainpack::RpcValue &rv)
{
	RpcSqlField ret;
	const RpcValue::Map &map = rv.asMap();
	ret.name = map.value("name").asString();
	return ret;
}

//==============================================
// RpcSqlResult
//==============================================
const RpcValue &RpcSqlResult::value(size_t row, size_t col) const
{
	if (row < rows.size()) {
		const auto &cells = rows[row];
		if (col < cells.size()) {
			return cells[col];
		}
	}
	static RpcValue s;
	return s;
}

const RpcValue& RpcSqlResult::value(size_t row, const std::string &name) const
{
	if (auto ix = columnIndex(name); ix.has_value()) {
		return value(row, ix.value());
	}
	static RpcValue s;
	return s;
}

void RpcSqlResult::setValue(size_t row, size_t col, const RpcValue &val)
{
	if (row < rows.size()) {
		auto &r = rows[row];
		if (col < r.size()) {
			r[col] = val;
		}
	}
}

void RpcSqlResult::setValue(size_t row, const std::string &name, const RpcValue &val)
{
	if (auto ix = columnIndex(name); ix.has_value()) {
		setValue(row, ix.value(), val);
	}
}

RpcValue::List RpcSqlResult::toRecordList() const
{
	RpcValue::List ret;
	for (const auto &row : rows) {
		SqlRecord rec;
		int n = 0;
		for (const auto &field : fields) {
			rec[field.name] = row[n++];
		}
		ret.push_back(rec);
	}
	return ret;
}

std::optional<size_t> RpcSqlResult::columnIndex(const std::string &name) const
{
	for (size_t col = 0; col < fields.size(); ++col) {
		const auto &fld = fields[col];
		if (fld.name == name) {
			return col;
		}
	}
	return {};
}

RpcValue RpcSqlResult::toRpcValue() const
{
	RpcValue::Map ret;
	if(isSelect()) {
		RpcValue::List flds;
		for(const auto &fld : this->fields)
			flds.push_back(fld.toRpcValue());
		ret["fields"] = flds;
		ret["rows"] = rows;
	}
	else {
		ret["numRowsAffected"] = numRowsAffected;
		ret["lastInsertId"] = lastInsertId.has_value()? RpcValue(lastInsertId.value()): RpcValue(nullptr);
	}
	return ret;
}

RpcSqlResult RpcSqlResult::fromRpcValue(const RpcValue &rv)
{
	RpcSqlResult ret;
	const auto &map = rv.asMap();
	const auto &flds = map.valref("fields").asList();
	if(flds.empty()) {
		ret.numRowsAffected = map.value("numRowsAffected").toInt();
		ret.lastInsertId = map.value("lastInsertId").toInt();
	}
	else {
		for(const auto &fv : flds) {
			ret.fields.push_back(RpcSqlField::fromRpcValue(fv));
		}
		for (const auto &row : map.value("rows").asList()) {
			ret.rows.push_back(row.asList());
		}
	}
	return ret;
}

SqlQueryAndParams SqlQueryAndParams::fromRpcValue(const shv::chainpack::RpcValue &rv)
{
	auto sql_query = rv.asList().valref(0).asString();
	const auto &sql_params = rv.asList().valref(0);
	return SqlQueryAndParams { .query = sql_query, .params = sql_params.asMap() };
}

RpcValue QxRecChng::toRpcValue() const
{
	RpcValue::Map ret;
	ret["table"] = table.toStdString();
	ret["id"] = id;
	ret["record"] = shv::coreqt::rpc::qVariantToRpcValue(record);
	auto rec_op_string = [](QxRecOp op) {
		switch (op) {
		case QxRecOp::Insert: return "Insert";
		case QxRecOp::Update: return "Update";
		case QxRecOp::Delete: return "Delete";
		}
		return "";
	};
	ret["op"] = rec_op_string(op);
	return ret;
}

//==============================================
// SqlApi
//==============================================
SqlApi::SqlApi(QObject *parent)
	: QObject{parent}
{

}

SqlApi *SqlApi::instance()
{
	static auto *api = new SqlApi(QCoreApplication::instance());
	return api;
}

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

RpcSqlResult rpcSqlQuery(const SqlQueryAndParams &params)
{
	qf::core::sql::Query q;
	q.prepare(QString::fromUtf8(params.query), qf::core::Exception::Throw);
	for (const auto &[k, v] : params.params) {
		bool ok;
		QVariant val = shv::coreqt::rpc::rpcValueToQVariant(v, &ok);
		if (!ok) {
			QF_EXCEPTION(QStringLiteral("Cannot convert SHV type: %1 to QVariant").arg(v.typeName()));
		}
		q.bindValue(':' + QString::fromStdString(k), val);
	}
	q.exec(qf::core::Exception::Throw);
	RpcSqlResult ret;
	if(q.isSelect()) {
		QSqlRecord rec = q.record();
		for (int i = 0; i < rec.count(); ++i) {
			QSqlField fld = rec.field(i);
			RpcSqlField rfld;
			rfld.name = fld.name().toStdString();
			// rfld.name.replace("__", ".");
			ret.fields.push_back(rfld);
		}
		while(q.next()) {
			RpcSqlResult::Row row;
			for (int i = 0; i < rec.count(); ++i) {
				const QVariant v = q.value(i);
				if (v.isNull()) {
					row.push_back(RpcValue(nullptr));
				}
				else {
					row.push_back(shv::coreqt::rpc::qVariantToRpcValue(v));
				}
				//shvError() << v << v.isNull() << jsv.toVariant() << jsv.toVariant().isNull();
			}
			ret.rows.push_back(row);
		}
	}
	else {
		ret.numRowsAffected = q.numRowsAffected();
		ret.lastInsertId = q.lastInsertId().toInt();
	}
	return ret;
}

}

RpcSqlResult SqlApi::exec(const SqlQueryAndParams &params)
{
	return rpcSqlQuery(params);
}

RpcSqlResult SqlApi::query(const SqlQueryAndParams &params)
{
	return rpcSqlQuery(params);
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

RpcSqlResult SqlApi::list(const std::string &table, const std::vector<std::string> &fields, std::optional<int64_t> ids_above, std::optional<int64_t> limit)
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
	auto res = rpcSqlQuery(SqlQueryAndParams { .query = sql_query.toStdString(), .params = {}});
	return res;
}

int64_t SqlApi::create(const std::string &table, const SqlRecord &record)
{
	QStringList fields;
	QStringList placeholders;
	for (const auto &[k, v] : record) {
		auto name = QString::fromStdString(k);
		fields << name;
		placeholders << ':' + name;
	}
	QString sql_query = QStringLiteral("INSERT INTO %1 (%2) VALUES (%3)")
			.arg(table)
			.arg(fields.join(','))
			.arg(placeholders.join(','));
	qf::core::sql::Query q;
	q.prepare(sql_query, qf::core::Exception::Throw);
	for (const auto &[k, v] : record) {
		q.bindValue(':' + QString::fromStdString(k), shv::coreqt::rpc::rpcValueToQVariant(v));
	}
	q.exec(qf::core::Exception::Throw);
	auto id = q.lastInsertId().toInt();
	emit SqlApi::instance()->recchng(QxRecChng {
		.table = QString::fromStdString(table),
		.id = id,
		.record = shv::coreqt::rpc::rpcValueToQVariant(record),
		.op = QxRecOp::Insert
	});
	return id;
}

std::optional<SqlRecord> SqlApi::read(const std::string &table, int64_t id, const std::vector<std::string> &fields)
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
	auto res = rpcSqlQuery(SqlQueryAndParams { .query = sql_query.toStdString(), .params = {}});
	auto lst = res.toRecordList();
	if (lst.empty()) {
		return {};
	}
	return lst[0].asMap();
}

bool SqlApi::update(const std::string &table, int64_t id, const SqlRecord &record)
{
	QStringList fields;
	for (const auto &[k, v] : record) {
		auto name = QString::fromStdString(k);
		fields << name + " = :" + name;
	}
	QString sql_query = QStringLiteral("UPDATE %1 SET %2 WHERE id = %3")
			.arg(table)
			.arg(fields.join(','))
			.arg(id);
	qf::core::sql::Query q;
	q.prepare(sql_query, qf::core::Exception::Throw);
	for (const auto &[k, v] : record) {
		auto qv = shv::coreqt::rpc::rpcValueToQVariant(v);
		q.bindValue(':' + QString::fromStdString(k), qv);
	}
	q.exec(qf::core::Exception::Throw);
	bool updated = q.numRowsAffected() == 1;
	if (updated) {
		emit SqlApi::instance()->recchng(QxRecChng {
			.table = QString::fromStdString(table),
			.id = id,
			.record = shv::coreqt::rpc::rpcValueToQVariant(record),
			.op = QxRecOp::Update
		});
	}
	return updated;
}

bool SqlApi::drop(const std::string &table, int64_t id)
{
	QString sql_query = QStringLiteral("DELETE FROM %1 WHERE id = %2")
			.arg(table)
			.arg(id);
	qf::core::sql::Query q;
	q.exec(sql_query, qf::core::Exception::Throw);
	bool is_drop = q.numRowsAffected() == 1;
	if (is_drop) {
		emit SqlApi::instance()->recchng(QxRecChng {
			.table = QString::fromStdString(table),
			.id = id,
			.record = {},
			.op = QxRecOp::Delete
		});
	}
	return is_drop;
}

}
