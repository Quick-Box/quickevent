#include "sqlapi.h"

#include <qf/core/log.h>
#include <qf/core/sql/qxsql.h>
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

RpcValue dbFieldToRpcValue(const DbField &fld)
{
	RpcValue::Map ret;
	ret["name"] = fld.name.toStdString();
	return RpcValue(std::move(ret));
}

RpcValue execResultToRpcValue(const ExecResult &res)
{
	RpcValue::Map ret;
	ret["numRowsAffected"] = res.numRowsAffected;
	ret["lastInsertId"] = res.lastInsertId.has_value()? RpcValue(res.lastInsertId.value()): RpcValue(nullptr);
	return ret;
}

RpcValue queryResultToRpcValue(const QueryResult &res)
{
	RpcValue::Map ret;
	RpcValue::List flds;
	for(const auto &fld : res.fields) {
		flds.push_back(dbFieldToRpcValue(fld));
	}
	RpcValue::List rpc_rows;
	for (const auto &row : res.rows) {
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

namespace {
RpcValue::List toShvRecordList(const QList<Record> &records)
{
	RpcValue::List ret;
	for (const auto &rec : records) {
		ret.push_back(shv::coreqt::rpc::qVariantToRpcValue(rec));
	}
	return ret;
}
QStringList toQStringList(const std::vector<std::string> &sl)
{
	QStringList qsl;
	for (const auto &s : sl) {
		qsl << QString::fromStdString(s);
	}
	return qsl;
}
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

// namespace {

// class Transaction
// {
// public:
// 	Transaction(QSqlDatabase db) : m_db(db) {
// 		if (!m_db.transaction()) {
// 			qfWarning() << "BEGIN transaction error:" << m_db.lastError().text();
// 			throw std::runtime_error("BEGIN transaction error");
// 		}
// 	}
// 	~Transaction() {
// 		if (m_inTransaction) {
// 			m_db.rollback();
// 		}
// 	}
// 	void commit() {
// 		if (!m_db.commit()) {
// 			qfWarning() << "COMMIT transaction error:" << m_db.lastError().text();
// 			throw std::runtime_error("COMMIT transaction error");
// 		}
// 		m_inTransaction = false;
// 	}
// private:
// 	QSqlDatabase m_db;
// 	bool m_inTransaction = true;
// };

// void bindParams(qf::core::sql::Query &q, const Record &params)
// {
// 	for (const auto &[k, v] : params.asKeyValueRange()) {
// 		q.bindValue(':' + k, v);
// 	}
// }

// QueryResult sqlQuery(const SqlQueryAndParams &params)
// {
// 	qf::core::sql::Query q;
// 	q.prepare(params.query, qf::core::Exception::Throw);
// 	bindParams(q, params.params);
// 	q.exec(qf::core::Exception::Throw);

// 	QueryResult ret;
// 	QSqlRecord rec = q.record();
// 	for (int i = 0; i < rec.count(); ++i) {
// 		QSqlField fld = rec.field(i);
// 		DbField rfld;
// 		rfld.name = fld.name();
// 		// rfld.name.replace("__", ".");
// 		ret.fields.push_back(rfld);
// 	}
// 	while(q.next()) {
// 		QueryResult::Row row;
// 		for (int i = 0; i < rec.count(); ++i) {
// 			row.push_back(q.value(i));
// 			//shvError() << v << v.isNull() << jsv.toVariant() << jsv.toVariant().isNull();
// 		}
// 		ret.rows.insert(ret.rows.size(), row);
// 	}
// 	return ret;
// }

// ExecResult sqlExec(const SqlQueryAndParams &params)
// {
// 	qf::core::sql::Query q;
// 	q.prepare(params.query, qf::core::Exception::Throw);
// 	bindParams(q, params.params);
// 	q.exec(qf::core::Exception::Throw);

// 	ExecResult ret;
// 	ret.numRowsAffected = q.numRowsAffected();
// 	ret.lastInsertId = q.lastInsertId().toInt();
// 	return ret;
// }

// }

ExecResult SqlApi::exec(const SqlQueryAndParams &params)
{
	auto *qxsql = qf::gui::framework::Application::instance()->qxSql();
	return qxsql->exec(params.query, params.params);
}

QueryResult SqlApi::query(const SqlQueryAndParams &params)
{
	auto *qxsql = qf::gui::framework::Application::instance()->qxSql();
	return qxsql->query(params.query, params.params);
}

void SqlApi::transaction(const std::string &query, const shv::chainpack::RpcValue::List &params)
{
	auto *qxsql = qf::gui::framework::Application::instance()->qxSql();
	QVariantList qparams;
	for (const auto &p : params) {
		qparams << shv::coreqt::rpc::rpcValueToQVariant(p);
	}
	return qxsql->transaction(QString::fromStdString(query), qparams);
}

RpcValue::List SqlApi::list(const std::string &table, const std::vector<std::string> &fields, std::optional<int64_t> ids_above, std::optional<int64_t> limit)
{
	auto *qxsql = qf::gui::framework::Application::instance()->qxSql();
	auto records = qxsql->listRecords(QString::fromStdString(table), toQStringList(fields), ids_above, limit);
	return toShvRecordList(records);
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
	auto *qxsql = qf::gui::framework::Application::instance()->qxSql();
	return qxsql->createRecord(QString::fromStdString(table), shv::coreqt::rpc::rpcValueToQVariant(record).toMap(), this);
}

std::optional<RpcValue::Map> SqlApi::read(const std::string &table, int64_t id, const std::vector<std::string> &fields)
{
	auto *qxsql = qf::gui::framework::Application::instance()->qxSql();
	if (auto rec = qxsql->readRecord(QString::fromStdString(table), id, toQStringList(fields)); rec.has_value()) {
		return shv::coreqt::rpc::qVariantToRpcValue(rec.value()).asMap();
	}
	return {};
}

bool SqlApi::update(const std::string &table, int64_t id, const RpcValue::Map &record)
{
	auto *qxsql = qf::gui::framework::Application::instance()->qxSql();
	return qxsql->updateRecord(QString::fromStdString(table), id, shv::coreqt::rpc::rpcValueToQVariant(record).toMap(), this);
}

bool SqlApi::drop(const std::string &table, int64_t id)
{
	auto *qxsql = qf::gui::framework::Application::instance()->qxSql();
	return qxsql->deleteRecord(QString::fromStdString(table), id, this);
}

}
