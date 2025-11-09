#include "sqlapinode.h"

#include <qf/core/exception.h>
#include <qf/core/sql/query.h>
#include <qf/core/log.h>

#include <shv/chainpack/rpc.h>
#include <shv/coreqt/rpc.h>
#include <shv/coreqt/data/rpcsqlresult.h>

#include <QSqlField>
#include <QSqlQuery>

using namespace shv::chainpack;
using namespace shv::coreqt::data;

namespace Event::services::qx {
namespace {

/*

RpcSqlResult RpcSqlResult::fromRpcValue(const shv::chainpack::RpcValue &rv)
{
	RpcSqlResult ret;
	const RpcValue::Map &map = rv.asMap();
	const RpcValue::List &flds = map.value("fields").asList();
	if(flds.empty()) {
		ret.numRowsAffected = map.value("numRowsAffected").toInt();
		ret.lastInsertId = map.value("lastInsertId").toInt();
	}
	else {

	}
	return ret;

}
*/
}

SqlApiNode::SqlApiNode(shv::iotqt::node::ShvNode *parent)
	: Super("sql", parent)
{

}

shv::chainpack::RpcValue::Map SqlApiNode::recordToMap(const QSqlRecord &rec)
{
	RpcValue::Map record;
	for (int i = 0; i < rec.count(); ++i) {
		QSqlField fld = rec.field(i);
		auto fld_name = fld.name();
		fld_name.replace("__", ".");
		record[fld_name.toStdString()] = shv::coreqt::rpc::qVariantToRpcValue(fld.value());
	}
	return record;
}

RpcSqlResult SqlApiNode::rpcSqlResultFromQuery(QSqlQuery &q)
{
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
				if (v.isNull())
					row.append(QVariant());
				else
					row.append(v);
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

static auto METH_EXEC_SQL = "execSql";

const std::vector<MetaMethod> &SqlApiNode::metaMethods()
{
	static std::vector<MetaMethod> meta_methods {
		methods::DIR,
		methods::LS,
		{METH_EXEC_SQL, MetaMethod::Flag::None, {}, "RpcValue", AccessLevel::Write},
	};
	return meta_methods;
}

RpcValue SqlApiNode::callMethod(const StringViewList &shv_path, const std::string &method, const shv::chainpack::RpcValue &params, const shv::chainpack::RpcValue &user_id)
{
	qfLogFuncFrame() << shv_path.join('/') << method;
	//eyascore::utils::UserId user_id = eyascore::utils::UserId::makeUserName(QString::fromStdString(rq.userId().toMap().value("userName").toString()));
	if(shv_path.empty()) {
		if(method == METH_EXEC_SQL) {
			qf::core::sql::Query q;
			QString qs = params.to<QString>();
			q.exec(qs, qf::core::Exception::Throw);
			auto res = rpcSqlResultFromQuery(q);
			return res.toRpcValue();
		}
	}
	return Super::callMethod(shv_path, method, params, user_id);
}

}
