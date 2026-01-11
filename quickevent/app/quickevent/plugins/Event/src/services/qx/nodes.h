#pragma once

#include "qxnode.h"

#include <qf/core/sql/querybuilder.h>

namespace Event::services::qx {

class DotAppNode : public QxNode
{
	Q_OBJECT

	using Super = QxNode;
public:
	explicit DotAppNode(shv::iotqt::node::ShvNode *parent) : Super(".app", parent) {}
private:
	//shv::chainpack::RpcValue callMethodRq(const shv::chainpack::RpcRequest &rq) override;
	const std::vector<shv::chainpack::MetaMethod> &metaMethods() override;
	shv::chainpack::RpcValue callMethod(const StringViewList &shv_path, const std::string &method, const shv::chainpack::RpcValue &params, const shv::chainpack::RpcValue &user_id) override;
};

}
