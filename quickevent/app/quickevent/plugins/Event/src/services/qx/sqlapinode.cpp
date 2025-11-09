#include "sqlapinode.h"

#include <qf/core/exception.h>
#include <qf/core/sql/query.h>
#include <qf/core/log.h>
#include <qf/core/sql/connection.h>

#include <shv/chainpack/rpc.h>
#include <shv/coreqt/rpc.h>
#include <shv/coreqt/data/rpcsqlresult.h>

#include <QSqlField>
#include <QSqlQuery>
#include <QSqlError>

using namespace shv::chainpack;
using namespace shv::coreqt::data;

namespace Event::services::qx {

SqlApiNode::SqlApiNode(shv::iotqt::node::ShvNode *parent)
	: Super("sql", parent)
{

}

namespace {
class Transaction
{
public:
	Transaction(QSqlDatabase db) : m_db(db) {
	}
	~Transaction() {
		if (m_inTransaction) {
			m_db.rollback();
		}
	}
	void begin() {
		if (!m_db.transaction()) {
			qfWarning() << "BEGIN transaction error:" << m_db.lastError().text();
			throw std::runtime_error("BEGIN transaction error");
		}
		m_inTransaction = true;
	}
	void commit() {
		if (m_inTransaction) {
			if (!m_db.commit()) {
				qfWarning() << "COMMIT transaction error:" << m_db.lastError().text();
				throw std::runtime_error("COMMIT transaction error");
			}
			m_inTransaction = false;
		}
	}
private:
	QSqlDatabase m_db;
	bool m_inTransaction = false;
};

RpcSqlResult rpcSqlQuery(const QString &query, const RpcValue &params, bool in_transaction = false)
{
	auto conn = qf::core::sql::Connection::forName();
	Transaction tranaction(conn);
	if (in_transaction) {
		tranaction.begin();
	}
	qf::core::sql::Query q(conn);
	q.prepare(query, qf::core::Exception::Throw);
	for (const auto &[k, v] : params.asMap()) {
		bool ok;
		QVariant val = shv::coreqt::rpc::rpcValueToQVariant(v, &ok);
		if (!ok) {
			QF_EXCEPTION(QStringLiteral("Cannot convert SHV type: %1 to QVariant").arg(v.typeName()));
		}
		q.bindValue(QString::fromStdString(k), val);
	}
	q.exec(qf::core::Exception::Throw);
	if (in_transaction) {
		tranaction.commit();
	}

	RpcSqlResult ret;
	if(q.isSelect()) {
		QSqlRecord rec = q.record();
		for (int i = 0; i < rec.count(); ++i) {
			QSqlField fld = rec.field(i);
			RpcSqlField rfld;
			rfld.name = fld.name();
			rfld.name.replace("__", ".");
			rfld.type = fld.metaType().id();
			ret.fields.append(rfld);
		}
		while(q.next()) {
			RpcSqlResult::Row row;
			for (int i = 0; i < rec.count(); ++i) {
				const QVariant v = q.value(i);
				if (v.isNull()) {
					row.append(QVariant::fromValue(nullptr));
				}
				else {
					row.append(v);
				}
				//shvError() << v << v.isNull() << jsv.toVariant() << jsv.toVariant().isNull();
			}
			ret.rows.insert(ret.rows.count(), row);
		}
	}
	else {
		ret.numRowsAffected = q.numRowsAffected();
		ret.lastInsertId = q.lastInsertId().toInt();
	}
	return ret;
}

auto METH_QUERY = "query";
auto METH_EXEC = "exec";
auto METH_TRANSACTION = "transaction";
auto METH_LIST = "list";
auto METH_CREATE = "create";
auto METH_READ = "read";
auto METH_UPDATE = "update";
auto METH_DELETE = "delete";
}

const std::vector<MetaMethod> &SqlApiNode::metaMethods()
{
	static std::vector<MetaMethod> meta_methods {
		methods::DIR,
		methods::LS,
		{METH_QUERY, MetaMethod::Flag::LargeResultHint, "[s:query,{s|i|b|t|n}:params]", "{{s:name}:fields,[[s|i|b|t|n]]:rows}", AccessLevel::Read},
		{METH_EXEC, MetaMethod::Flag::None, "[s:query,{s|i|b|t|n}:params]", "{i:rows_affected,i|n:insert_id}", AccessLevel::Write},
		{METH_TRANSACTION, MetaMethod::Flag::None, "[s:query,{s|i|b|t|n}:params]", "n", AccessLevel::Write},
		{METH_LIST, MetaMethod::Flag::LargeResultHint, "{s:table,[s]|n:fields,i|n:ids_above,i|n:limit}", "[{s|i|b|t|n}]", AccessLevel::Write},
		{METH_CREATE, MetaMethod::Flag::None, "{s:table,{s|i|b|t|n}:record}", "i", AccessLevel::Write},
		{METH_READ, MetaMethod::Flag::None, "{s:table,i:id,{s}|n:fields}", "{s|i|b|t|n}|n", AccessLevel::Read},
		{METH_UPDATE, MetaMethod::Flag::None, "{s:table,i:id,{s|i|b|t|n}:record}", "b", AccessLevel::Write},
		{METH_DELETE, MetaMethod::Flag::None, "{s:table,i:id}", "b", AccessLevel::Write},
	};
	return meta_methods;
}

RpcValue SqlApiNode::callMethod(const StringViewList &shv_path, const std::string &method, const shv::chainpack::RpcValue &params, const shv::chainpack::RpcValue &user_id)
{
	qfLogFuncFrame() << shv_path.join('/') << method;
	//eyascore::utils::UserId user_id = eyascore::utils::UserId::makeUserName(QString::fromStdString(rq.userId().toMap().value("userName").toString()));
	if(shv_path.empty()) {
		if(method == METH_EXEC) {
			auto sql_query = params.asList().valref(0).to<QString>();
			const auto &sql_params = params.asList().valref(0);
			auto res = rpcSqlQuery(sql_query, sql_params);
			return res.toRpcValue();
		}
		if(method == METH_QUERY) {
			auto sql_query = params.asList().valref(0).to<QString>();
			const auto &sql_params = params.asList().valref(0);
			auto res = rpcSqlQuery(sql_query, sql_params);
			return res.toRpcValue();
		}
		if(method == METH_TRANSACTION) {
			auto sql_query = params.asList().valref(0).to<QString>();
			const auto &sql_params = params.asList().valref(0);
			auto res = rpcSqlQuery(sql_query, sql_params, true);
			return RpcValue(nullptr);
		}
		if(method == METH_LIST) {
			const auto &map = params.asMap();
			QStringList fields;
			for (const auto &fn : map.valref("fields").asList()) {
				fields << fn.to<QString>();
			}
			if (fields.isEmpty()) {
				fields << "*";
			}
			QString sql_query = QStringLiteral("SELECT %1 FROM %2").arg(fields.join(',')).arg(map.value("table").to<QString>());
			if (auto ids_above = map.value("ids_above"); ids_above.isInt()) {
				sql_query += " WHERE id > " + QString::number(ids_above.toInt());
			}
			if (auto limit = map.value("limit"); limit.isInt()) {
				sql_query += " LIMIT " + QString::number(limit.toInt());
			}
			auto res = rpcSqlQuery(sql_query, {});
			if (res.rows.isEmpty()) {
				return RpcValue(nullptr);
			}
			RpcValue::List ret;
			for (const auto &row : res.rows) {
				auto cells = row.toList();
				RpcValue::Map rec;
				int n = 0;
				for (const auto &field : res.fields) {
					rec[field.name.toStdString()] = shv::coreqt::rpc::qVariantToRpcValue(cells.value(n++));
				}
				ret.push_back(rec);
			}
			return ret;
		}
		if(method == METH_CREATE) {
			const auto &map = params.asMap();
			auto table = map.valref("table").to<QString>();
			const auto &record = map.valref("record").asMap();
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
				q.bindValue(QString::fromStdString(k), shv::coreqt::rpc::rpcValueToQVariant(v));
			}
			q.exec(qf::core::Exception::Throw);
			return q.lastInsertId().toInt();
		}
		if(method == METH_READ) {
			const auto &map = params.asMap();
			QStringList fields;
			for (const auto &fn : map.valref("fields").asList()) {
				fields << fn.to<QString>();
			}
			if (fields.isEmpty()) {
				fields << "*";
			}
			QString sql_query = QStringLiteral("SELECT %1 FROM %2 WHERE id = %3")
					.arg(fields.join(','))
					.arg(map.value("table").to<QString>())
					.arg(map.value("id").toInt()) ;
			auto res = rpcSqlQuery(sql_query, {});
			if (res.rows.isEmpty()) {
				return RpcValue(nullptr);
			}
			RpcValue::Map rec;
			auto cells = res.rows[0].toList();
			int n = 0;
			for (const auto &field : res.fields) {
				rec[field.name.toStdString()] = shv::coreqt::rpc::qVariantToRpcValue(cells.value(n++));
			}
			return rec;
		}
		if(method == METH_UPDATE) {
			const auto &map = params.asMap();
			auto table = map.valref("table").to<QString>();
			auto id = map.valref("id").toInt();
			const auto &record = map.valref("record").asMap();
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
				q.bindValue(QString::fromStdString(k), shv::coreqt::rpc::rpcValueToQVariant(v));
			}
			q.exec(qf::core::Exception::Throw);
			return q.numRowsAffected() == 1;
		}
		if(method == METH_DELETE) {
			const auto &map = params.asMap();
			auto table = map.valref("table").to<QString>();
			auto id = map.valref("id").toInt();
			QString sql_query = QStringLiteral("DELETE FROM %1 WHERE id = %2")
					.arg(table)
					.arg(id);
			qf::core::sql::Query q;
			q.exec(sql_query, qf::core::Exception::Throw);
			return q.numRowsAffected() == 1;
		}
	}
	return Super::callMethod(shv_path, method, params, user_id);
}

}
