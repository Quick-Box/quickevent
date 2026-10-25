#include "qxnode.h"

#include <qf/core/exception.h>
#include <qf/core/log.h>

#include <shv/chainpack/rpc.h>

using namespace shv::chainpack;

namespace Event::services::qx {

QxNode::QxNode(const std::string &name, shv::iotqt::node::ShvNode *parent)
	: Super(name, parent)
{

}

size_t QxNode::methodCount(const StringViewList &shv_path)
{
	if(shv_path.empty()) {
		return metaMethods().size();
	}
	return Super::methodCount(shv_path);
}

const MetaMethod *QxNode::metaMethod(const StringViewList &shv_path, size_t ix)
{
	if(shv_path.empty()) {
		if(metaMethods().size() <= ix)
			QF_EXCEPTION("Invalid method index: " + QString::number(ix) + " of: " + QString::number(metaMethods().size()));
		return &(metaMethods()[ix]);
	}
	return Super::metaMethod(shv_path, ix);
}

}
