#pragma once

#include <shv/iotqt/node/shvnode.h>

namespace Event::services::qx {

class QxNode : public shv::iotqt::node::ShvNode
{
	Q_OBJECT

	using Super = shv::iotqt::node::ShvNode;
public:
	explicit QxNode(const std::string &name, shv::iotqt::node::ShvNode *parent);

	size_t methodCount(const StringViewList &shv_path) override;
	const shv::chainpack::MetaMethod* metaMethod(const StringViewList &shv_path, size_t ix) override;
protected:
	//shv::chainpack::RpcValue callMethodRq(const shv::chainpack::RpcRequest &rq) override;
	//shv::chainpack::RpcValue callMethod(const StringViewList &shv_path, const std::string &method, const shv::chainpack::RpcValue &params, const shv::chainpack::RpcValue &user_id) override;
	virtual const std::vector<shv::chainpack::MetaMethod> &metaMethods() = 0;
};

}
