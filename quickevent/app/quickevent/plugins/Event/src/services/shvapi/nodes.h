#pragma once

#include "shvnode.h"

#include <qf/core/sql/querybuilder.h>

namespace Event::services::shvapi {

class DotAppNode : public shvapi::ShvNode
{
	Q_OBJECT

	using Super = shvapi::ShvNode;
public:
	explicit DotAppNode(shv::iotqt::node::ShvNode *parent) : Super(".app", parent) {}
private:
	//shv::chainpack::RpcValue callMethodRq(const shv::chainpack::RpcRequest &rq) override;
	const std::vector<shv::chainpack::MetaMethod> &metaMethods() override;
	shv::chainpack::RpcValue callMethod(const StringViewList &shv_path, const std::string &method, const shv::chainpack::RpcValue &params, const shv::chainpack::RpcValue &user_id) override;
};

class EventNode : public shvapi::ShvNode
{
	Q_OBJECT

	using Super = shvapi::ShvNode;
public:
	explicit EventNode(shv::iotqt::node::ShvNode *parent);
private:
	const std::vector<shv::chainpack::MetaMethod> &metaMethods() override;
	shv::chainpack::RpcValue callMethod(const StringViewList &shv_path, const std::string &method, const shv::chainpack::RpcValue &params, const shv::chainpack::RpcValue &user_id) override;
};

class SqlViewNode : public shvapi::ShvNode
{
	Q_OBJECT

	using Super = shvapi::ShvNode;
public:
	explicit SqlViewNode(const std::string &name, shv::iotqt::node::ShvNode *parent)
		: Super(name, parent)
	{}
	void setQueryBuilder(const qf::core::sql::QueryBuilder &qb);
protected:
	virtual qf::core::sql::QueryBuilder effectiveQueryBuilder();
private:
	const std::vector<shv::chainpack::MetaMethod> &metaMethods() override;
	shv::chainpack::RpcValue callMethod(const StringViewList &shv_path, const std::string &method, const shv::chainpack::RpcValue &params, const shv::chainpack::RpcValue &user_id) override;
private:
	qf::core::sql::QueryBuilder m_queryBuilder;
};

class CurrentStageSqlViewNode : public SqlViewNode
{
	Q_OBJECT

	using Super = SqlViewNode;
public:
	explicit CurrentStageSqlViewNode(const std::string &name, shv::iotqt::node::ShvNode *parent)
		: Super(name, parent)
	{}
protected:
	qf::core::sql::QueryBuilder effectiveQueryBuilder() override;
};

}
