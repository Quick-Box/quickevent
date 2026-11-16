#pragma once

#include "qxnode.h"

class QSqlQuery;
class QSqlRecord;

namespace shv::coreqt::data { class RpcSqlResult; }

namespace Event::services::qx {

class SqlApi;

class SqlApiNode : public QxNode
{
	Q_OBJECT

	using Super = QxNode;
public:
	explicit SqlApiNode(shv::iotqt::node::ShvNode *parent);

	// While postponed, recchng signals are queued instead of being sent and they are sent
	// in the original order when the last postpone is released. Calls can be nested.
	void postponeRecChngSignals();
	void releaseRecChngSignals();
protected:
	const std::vector<shv::chainpack::MetaMethod> &metaMethods() override;
	shv::chainpack::RpcValue callMethod(const StringViewList &shv_path, const std::string &method, const shv::chainpack::RpcValue &params, const shv::chainpack::RpcValue &user_id) override;
private:
	void sendRecChngSignal(const shv::chainpack::RpcValue &params);
private:
	SqlApi *m_sqlApi = nullptr;
	int m_postponeRecChngCount = 0;
	std::vector<shv::chainpack::RpcValue> m_postponedRecChngs;
};

}
