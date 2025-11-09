#pragma once

#include "qxnode.h"

class QSqlQuery;
class QSqlRecord;

namespace shv::coreqt::data { class RpcSqlResult; }

namespace Event::services::qx {

class SqlApiNode : public QxNode
{
	Q_OBJECT

	using Super = QxNode;
public:
	explicit SqlApiNode(shv::iotqt::node::ShvNode *parent);

	static shv::coreqt::data::RpcSqlResult rpcSqlResultFromQuery(QSqlQuery &q);
	static shv::chainpack::RpcValue::Map recordToMap(const QSqlRecord &rec);
protected:
	const std::vector<shv::chainpack::MetaMethod> &metaMethods() override;
	shv::chainpack::RpcValue callMethod(const StringViewList &shv_path, const std::string &method, const shv::chainpack::RpcValue &params, const shv::chainpack::RpcValue &user_id) override;
};

}
