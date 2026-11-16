#pragma once

#include "qxnode.h"

#include <optional>
#include <vector>

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
	// in the original order when released.
	void setRecChngSignalsPostponed(bool is_postponed);

	shv::chainpack::RpcValue callMethod(const StringViewList &shv_path, const std::string &method, const shv::chainpack::RpcValue &params, const shv::chainpack::RpcValue &user_id) override;
protected:
	const std::vector<shv::chainpack::MetaMethod> &metaMethods() override;
private:
	void sendRecChngSignal(const shv::chainpack::RpcValue &params);
private:
	SqlApi *m_sqlApi = nullptr;
	// nullopt: send immediately, otherwise collect until released
	std::optional<std::vector<shv::chainpack::RpcValue>> m_postponedRecChngs;
};

}
