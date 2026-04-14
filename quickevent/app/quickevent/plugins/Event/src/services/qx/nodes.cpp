#include "nodes.h"

#include <qf/gui/framework/mainwindow.h>
#include <qf/core/exception.h>
#include <qf/core/log.h>
#include <qf/core/sql/query.h>

#include <shv/chainpack/rpc.h>
#include <shv/coreqt/rpc.h>
#include <shv/coreqt/data/rpcsqlresult.h>

#include <QSqlField>

using namespace qf::core::sql;
using namespace shv::chainpack;

namespace Event::services::qx {

//=========================================================
// DotAppNode
//=========================================================
namespace {
auto METH_NAME = "name";
}
const std::vector<shv::chainpack::MetaMethod> &DotAppNode::metaMethods()
{
	static std::vector<MetaMethod> meta_methods {
		methods::DIR,
		methods::LS,
		{Rpc::METH_PING, MetaMethod::Flag::None, {}, "RpcValue", AccessLevel::Browse},
		{METH_NAME, MetaMethod::Flag::IsGetter, {}, "RpcValue", AccessLevel::Browse},
	};
	return meta_methods;
}

RpcValue DotAppNode::callMethod(const StringViewList &shv_path, const std::string &method, const shv::chainpack::RpcValue &params, const shv::chainpack::RpcValue &user_id)
{
	qfLogFuncFrame() << shv_path.join('/') << method;
	//eyascore::utils::UserId user_id = eyascore::utils::UserId::makeUserName(QString::fromStdString(rq.userId().toMap().value("userName").toString()));
	if(shv_path.empty()) {
		if(method == Rpc::METH_PING) {
			return nullptr;
		}
		if(method == METH_NAME) {
			return "QuickEvent";
		}
	}
	return Super::callMethod(shv_path, method, params, user_id);
}

}
