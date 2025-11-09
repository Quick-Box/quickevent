#include "nodes.h"

#include "sqlapinode.h"
#include "../../eventplugin.h"
#include "../../../../Runs/src/runsplugin.h"

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
using qf::gui::framework::getPlugin;
using Event::EventPlugin;
using Runs::RunsPlugin;

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

//=========================================================
// EventNode
//=========================================================
static const auto METH_CURRENT_STAGE = "currentStage";
static const auto METH_EVENT_CONFIG = "eventConfig";

EventNode::EventNode(shv::iotqt::node::ShvNode *parent)
: Super("event", parent)
{
}

const std::vector<MetaMethod> &EventNode::metaMethods()
{
	static std::vector<MetaMethod> meta_methods {
		methods::DIR,
		methods::LS,
		{METH_NAME, MetaMethod::Flag::IsGetter, {}, "RpcValue", AccessLevel::Read},
		{METH_CURRENT_STAGE, MetaMethod::Flag::IsGetter, {}, "RpcValue", AccessLevel::Read},
		{METH_EVENT_CONFIG, MetaMethod::Flag::IsGetter, {}, "RpcValue", AccessLevel::Read},
	};
	return meta_methods;
}

RpcValue EventNode::callMethod(const StringViewList &shv_path, const std::string &method, const shv::chainpack::RpcValue &params, const shv::chainpack::RpcValue &user_id)
{
	qfLogFuncFrame() << shv_path.join('/') << method;
	if(shv_path.empty()) {
		if(method == METH_NAME) {
			return getPlugin<EventPlugin>()->eventConfig()->eventName().toStdString();
		}
		if(method == METH_CURRENT_STAGE) {
			return getPlugin<EventPlugin>()->currentStageId();
		}
		if(method == METH_EVENT_CONFIG) {
			auto cfg = getPlugin<EventPlugin>()->eventConfig();
			return shv::coreqt::rpc::qVariantToRpcValue(cfg->values());
		}
	}
	return Super::callMethod(shv_path, method, params, user_id);
}

//=========================================================
// StartListNode
//=========================================================
static const auto METH_TABLE = "table";
static const auto METH_RECORD = "record";
//static const auto SIG_REC_CHNG = "recchng";

void SqlViewNode::setQueryBuilder(const qf::core::sql::QueryBuilder &qb)
{
	m_queryBuilder = qb;
}

qf::core::sql::QueryBuilder SqlViewNode::effectiveQueryBuilder()
{
	return m_queryBuilder;
}

const std::vector<MetaMethod> &SqlViewNode::metaMethods()
{
	static std::vector<MetaMethod> meta_methods {
		methods::DIR,
		methods::LS,
		{METH_TABLE, MetaMethod::Flag::None, {}, "RpcValue", AccessLevel::Read},
		{METH_RECORD, MetaMethod::Flag::None, "RpcValue", "RpcValue", AccessLevel::Read },
		//{METH_SET_RECORD, MetaMethod::Flag::None, "RpcValue", {}, AccessLevel::Write},
	};
	return meta_methods;
}

RpcValue SqlViewNode::callMethod(const StringViewList &shv_path, const std::string &method, const shv::chainpack::RpcValue &params, const shv::chainpack::RpcValue &user_id)
{
	qfLogFuncFrame() << shv_path.join('/') << method;
	//eyascore::utils::UserId user_id = eyascore::utils::UserId::makeUserName(QString::fromStdString(rq.userId().toMap().value("userName").toString()));
	if(shv_path.empty()) {
		if(method == METH_TABLE) {
			const auto &m = params.asMap();
			auto qb = effectiveQueryBuilder();
			if (auto where = m.value("where").to<QString>(); !where.isEmpty()) {
				qb.where(where);
			}
			if (auto order_by = m.value("orderBy").to<QString>(); !order_by.isEmpty()) {
				qb.orderBy(order_by);
			}
			qf::core::sql::Query q;
			QString qs = qb.toString();
			q.exec(qs, qf::core::Exception::Throw);
			auto res = SqlApiNode::rpcSqlResultFromQuery(q);
			return res.toRpcValue();
		}
		if(method == METH_RECORD) {
			auto id = params.toInt();
			auto qb = effectiveQueryBuilder();
			qb.where("runs.id = " + QString::number(id));
			qf::core::sql::Query q;
			QString qs = qb.toString();
			qfDebug() << qs;
			q.exec(qs, qf::core::Exception::Throw);
			if (q.next()) {
				return SqlApiNode::recordToMap(q.record());
			}
			return RpcValue::Map{};
		}
	}
	return Super::callMethod(shv_path, method, params, user_id);
}

//=========================================================
// CurrentStageConfigNode
//=========================================================
const std::vector<MetaMethod> &CurrentStageConfigNode::metaMethods()
{
	static std::vector<MetaMethod> meta_methods {
		methods::DIR,
		methods::LS,
		{Rpc::METH_GET, MetaMethod::Flag::IsGetter, {}, "Map", AccessLevel::Read},
	};
	return meta_methods;
}

RpcValue CurrentStageConfigNode::callMethod(const StringViewList &shv_path, const std::string &method, const shv::chainpack::RpcValue &params, const shv::chainpack::RpcValue &user_id)
{
	qfLogFuncFrame() << shv_path.join('/') << method;
	//eyascore::utils::UserId user_id = eyascore::utils::UserId::makeUserName(QString::fromStdString(rq.userId().toMap().value("userName").toString()));
	if(shv_path.empty()) {
		if(method == Rpc::METH_GET) {
			auto *event_plugin = getPlugin<EventPlugin>();
			QVariantMap data = event_plugin->stageData(event_plugin->currentStageId());
			data.remove("drawingConfig"); // remove internal key
			return shv::coreqt::rpc::qVariantToRpcValue(data);
		}
	}
	return Super::callMethod(shv_path, method, params, user_id);
}

//=========================================================
// CurrentStageStartListNode
//=========================================================
CurrentStageStartListNode::CurrentStageStartListNode(shv::iotqt::node::ShvNode *parent)
	: Super("startList", parent)
{
	auto qb = getPlugin<RunsPlugin>()->startListQuery();
	//qb.orderBy("runs.startTimeMs");
	setQueryBuilder(qb);
}

qf::core::sql::QueryBuilder CurrentStageStartListNode::effectiveQueryBuilder()
{
	auto qb = Super::effectiveQueryBuilder();
	auto *event_plugin = getPlugin<EventPlugin>();
	qb.where(QStringLiteral("runs.stageId=%1").arg(event_plugin->currentStageId()));
	return qb;
}

//=========================================================
// CurrentStageRunsNode
//=========================================================
CurrentStageRunsNode::CurrentStageRunsNode(shv::iotqt::node::ShvNode *parent)
	: Super("runs", parent)
{
}

QueryBuilder CurrentStageRunsNode::effectiveQueryBuilder()
{
	auto *event_plugin = getPlugin<EventPlugin>();
	auto qb = getPlugin<RunsPlugin>()->runsQuery(event_plugin->currentStageId());
	return qb;
}

static const auto METH_SET_RECORD = "setRecord";
//static const auto METH_RUN_CHANGED = "runchng";
static const auto SIG_RUN_CHANGED = "runchng";

const std::vector<MetaMethod> &CurrentStageRunsNode::metaMethods()
{
	static std::vector<MetaMethod> meta_methods {
		methods::DIR,
		methods::LS,
		{METH_TABLE, MetaMethod::Flag::None, {}, "RpcValue", AccessLevel::Read},
		{METH_RECORD, MetaMethod::Flag::None, "Int", "Map", AccessLevel::Read, {{SIG_RUN_CHANGED, "[Int, RpcValue]"}} },
		{METH_SET_RECORD, MetaMethod::Flag::None, "[Int, RpcValue]", {}, AccessLevel::Write},
	};
	return meta_methods;
	static auto s_meta_methods = [this]() {
		auto mm = Super::metaMethods();
		mm.push_back({METH_SET_RECORD, MetaMethod::Flag::None, "[int, Map]", {}, AccessLevel::Write });
		return mm;
	}();
	return s_meta_methods;
}

RpcValue CurrentStageRunsNode::callMethod(const StringViewList &shv_path, const std::string &method, const shv::chainpack::RpcValue &params, const shv::chainpack::RpcValue &user_id)
{
	qfLogFuncFrame() << shv_path.join('/') << method;
	if(shv_path.empty()) {
		if(method == METH_SET_RECORD) {
			auto *plugin = getPlugin<RunsPlugin>();
			const auto &lst = params.asList();
			plugin->setRunsRecord(lst.value(0).toInt(), shv::coreqt::rpc::rpcValueToQVariant(lst.value(1)));
			return nullptr;
		}
	}
	return Super::callMethod(shv_path, method, params, user_id);
}

void CurrentStageRunsNode::sendRunChangedSignal(const QVariant &qparam)
{
	auto param = shv::coreqt::rpc::qVariantToRpcValue(qparam);
	Q_ASSERT(param.isList());
	Q_ASSERT(param.asList().value(0).toInt() > 0); // run.id
	RpcSignal sig;
	sig.setShvPath(shvPath().asString());
	sig.setMethod(SIG_RUN_CHANGED);
	sig.setSource(METH_RECORD);
	sig.setParams(param);
	qfDebug() << "emit:" << sig.toPrettyString();
	emitSendRpcMessage(sig);
}

//=========================================================
// CurrentStageClassesNode
//=========================================================
CurrentStageClassesNode::CurrentStageClassesNode(shv::iotqt::node::ShvNode *parent)
	: Super("classes", parent)
{
}

QueryBuilder CurrentStageClassesNode::effectiveQueryBuilder()
{
	auto *event_plugin = getPlugin<EventPlugin>();
	QueryBuilder qb;
	qb.select2("classes", "*")
			.select2("classdefs", "*")
			.select2("courses", "id, name, length, climb")
			.from("classes")
			.joinRestricted("classes.id", "classdefs.classId", "classdefs.stageId=" QF_IARG(event_plugin->currentStageId()))
			.join("classdefs.courseId", "courses.id")
			.orderBy("classes.name");//.limit(10);
	return qb;
}


}
