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

class EventNode : public QxNode
{
	Q_OBJECT

	using Super = QxNode;
public:
	explicit EventNode(shv::iotqt::node::ShvNode *parent);
private:
	const std::vector<shv::chainpack::MetaMethod> &metaMethods() override;
	shv::chainpack::RpcValue callMethod(const StringViewList &shv_path, const std::string &method, const shv::chainpack::RpcValue &params, const shv::chainpack::RpcValue &user_id) override;
};

class SqlViewNode : public QxNode
{
	Q_OBJECT

	using Super = QxNode;
public:
	explicit SqlViewNode(const std::string &name, shv::iotqt::node::ShvNode *parent)
		: Super(name, parent)
	{}
	void setQueryBuilder(const qf::core::sql::QueryBuilder &qb);
protected:
	virtual qf::core::sql::QueryBuilder effectiveQueryBuilder();
	const std::vector<shv::chainpack::MetaMethod> &metaMethods() override;
	shv::chainpack::RpcValue callMethod(const StringViewList &shv_path, const std::string &method, const shv::chainpack::RpcValue &params, const shv::chainpack::RpcValue &user_id) override;
private:
	qf::core::sql::QueryBuilder m_queryBuilder;
};

class CurrentStageConfigNode : public QxNode
{
	Q_OBJECT

	using Super = QxNode;
public:
	explicit CurrentStageConfigNode(shv::iotqt::node::ShvNode *parent)
		: Super("config", parent)
	{}
protected:
	const std::vector<shv::chainpack::MetaMethod> &metaMethods() override;
	shv::chainpack::RpcValue callMethod(const StringViewList &shv_path, const std::string &method, const shv::chainpack::RpcValue &params, const shv::chainpack::RpcValue &user_id) override;
};

class CurrentStageStartListNode : public SqlViewNode
{
	Q_OBJECT

	using Super = SqlViewNode;
public:
	explicit CurrentStageStartListNode(shv::iotqt::node::ShvNode *parent);
protected:
	qf::core::sql::QueryBuilder effectiveQueryBuilder() override;
};

class CurrentStageRunsNode : public SqlViewNode
{
	Q_OBJECT

	using Super = SqlViewNode;
public:
	explicit CurrentStageRunsNode(shv::iotqt::node::ShvNode *parent);
	void sendRunChangedSignal(const QVariant &qparam);
protected:
	const std::vector<shv::chainpack::MetaMethod> &metaMethods() override;
	shv::chainpack::RpcValue callMethod(const StringViewList &shv_path, const std::string &method, const shv::chainpack::RpcValue &params, const shv::chainpack::RpcValue &user_id) override;
	qf::core::sql::QueryBuilder effectiveQueryBuilder() override;
};

class CurrentStageClassesNode : public SqlViewNode
{
	Q_OBJECT

	using Super = SqlViewNode;
public:
	explicit CurrentStageClassesNode(shv::iotqt::node::ShvNode *parent);

protected:
	qf::core::sql::QueryBuilder effectiveQueryBuilder() override;
};

}
