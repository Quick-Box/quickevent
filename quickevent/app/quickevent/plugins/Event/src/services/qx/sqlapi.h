#pragma once

#include <shv/chainpack/rpcvalue.h>
#include <qf/core/sql/qxsql.h>
#include <qf/gui/model/sqltablemodel.h>

#include <QObject>
#include <QVariantMap>

namespace qf::core::sql { struct QxRecChng; }

namespace Event::services::qx {

using DbField = qf::core::sql::DbField;
using ExecResult = qf::core::sql::ExecResult;
using QueryResult = qf::core::sql::QueryResult;

shv::chainpack::RpcValue dbFieldToRpcValue(const DbField &fld);
shv::chainpack::RpcValue execResultToRpcValue(const ExecResult &res);
shv::chainpack::RpcValue queryResultToRpcValue(const QueryResult &res);

using Record = QVariantMap;

struct SqlQueryAndParams
{
	QString query;
	Record params;

	static SqlQueryAndParams fromRpcValue(const shv::chainpack::RpcValue &rv);
};

shv::chainpack::RpcValue qxRecChngToRpcValue(const qf::core::sql::QxRecChng &chng);


class SqlApi : public QObject
{
	Q_OBJECT
public:
	explicit SqlApi(QObject *parent = nullptr);

	Q_SIGNAL void recChng(const qf::core::sql::QxRecChng &recchng, QObject *source);

	ExecResult exec(const SqlQueryAndParams &params);
	QueryResult query(const SqlQueryAndParams &params);
	void transaction(const std::string &query, const shv::chainpack::RpcValue::List &params);
	shv::chainpack::RpcValue::List list(const std::string &table, const std::vector<std::string> &fields, std::optional<int64_t> ids_above, std::optional<int64_t> limit);
	int64_t create(const std::string &table, const shv::chainpack::RpcValue::Map &record);
	std::optional<shv::chainpack::RpcValue::Map> read(const std::string &table, int64_t id, const std::vector<std::string> &fields);
	bool update(const std::string &table, int64_t id, const shv::chainpack::RpcValue::Map &record);
	bool drop(const std::string &table, int64_t id);
};

}
