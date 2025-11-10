#include "sqlapinode.h"
#include "sqlapi.h"

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
		{METH_TRANSACTION, MetaMethod::Flag::None, "[s:query,[{s|i|b|t|n}]:params]", "n", AccessLevel::Write},
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
			auto res = SqlApi::exec(SqlQueryAndParams::fromRpcValue(params));
			return res.toRpcValue();
		}
		if(method == METH_QUERY) {
			auto res = SqlApi::exec(SqlQueryAndParams::fromRpcValue(params));
			return res.toRpcValue();
		}
		if(method == METH_TRANSACTION) {
			auto sql_query = params.asList().valref(0).asString();
			const auto &sql_params = params.asList().valref(0);
			SqlApi::transaction(sql_query, sql_params.asList());
			return RpcValue(nullptr);
		}
		if(method == METH_LIST) {
			const auto &map = params.asMap();
			const auto &table = map.value("table").asString();
			std::vector<std::string> fields;
			for (const auto &fn : map.valref("fields").asList()) {
				fields.push_back(fn.asString());
			}
			auto ids_above = map.contains("ids_above")? std::optional<int64_t>(map.value("ids_above").toInt64()): std::optional<int64_t>();
			auto limit = map.contains("limit")? std::optional<int64_t>(map.value("limit").toInt64()): std::optional<int64_t>();
			auto res = SqlApi::list(table, fields, ids_above, limit);
			return res.toRecordList();
		}
		if(method == METH_CREATE) {
			const auto &map = params.asMap();
			const auto &table = map.value("table").asString();
			const auto &record = map.valref("record").asMap();
			auto res = SqlApi::create(table, record);
			return res;
		}
		if(method == METH_READ) {
			const auto &map = params.asMap();
			const auto &table = map.value("table").asString();
			auto id = map.value("id").toInt64();
			std::vector<std::string> fields;
			for (const auto &fn : map.valref("fields").asList()) {
				fields.push_back(fn.asString());
			}
			auto res = SqlApi::read(table, id, fields);
			if (res.has_value()) {
				return res.value();
			}
			return RpcValue(nullptr);
		}
		if(method == METH_UPDATE) {
			const auto &map = params.asMap();
			const auto &table = map.value("table").asString();
			auto id = map.value("id").toInt64();
			const auto &record = map.valref("record").asMap();
			auto res = SqlApi::update(table, id, record);
			return res;
		}
		if(method == METH_DELETE) {
			const auto &map = params.asMap();
			const auto &table = map.value("table").asString();
			auto id = map.valref("id").toInt();
			auto res = SqlApi::drop(table, id);
			return res;
		}
	}
	return Super::callMethod(shv_path, method, params, user_id);
}

}
