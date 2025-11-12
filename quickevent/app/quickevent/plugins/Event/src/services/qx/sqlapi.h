#pragma once

#include <shv/chainpack/rpcvalue.h>

#include <QObject>
#include <QVariantMap>

namespace Event::services::qx {

struct RpcSqlField
{
	std::string name;

	//explicit RpcSqlField(const QJsonObject &jo = QJsonObject()) : Super(jo) {}
	shv::chainpack::RpcValue toRpcValue() const;
	// QVariant toVariant() const;
	static RpcSqlField fromRpcValue(const shv::chainpack::RpcValue &rv);
	// static RpcSqlField fromVariant(const QVariant &v);
};

struct RpcSqlResult
{
	int numRowsAffected = 0;
	std::optional<int> lastInsertId = 0;
	std::vector<RpcSqlField> fields;
	using Row = shv::chainpack::RpcValue::List;
	std::vector<Row> rows;

	RpcSqlResult() = default;
	// explicit RpcSqlResult(const QSqlQuery &q);

	std::optional<size_t> columnIndex(const std::string &name) const;
	const shv::chainpack::RpcValue& value(size_t row, size_t col) const;
	const shv::chainpack::RpcValue& value(size_t row, const std::string &name) const;
	void setValue(size_t row, size_t col, const shv::chainpack::RpcValue &val);
	void setValue(size_t row, const std::string &name, const shv::chainpack::RpcValue &val);

	bool isSelect() const {return !fields.empty();}
	shv::chainpack::RpcValue toRpcValue() const;
	shv::chainpack::RpcValue::List toRecordList() const;
	// QVariant toVariant() const;
	// static RpcSqlResult fromVariant(const QVariant &v);
	static RpcSqlResult fromRpcValue(const shv::chainpack::RpcValue &rv);
};

using SqlRecord = shv::chainpack::RpcValue::Map;

struct SqlQueryAndParams
{
	std::string query;
	SqlRecord params;

	static SqlQueryAndParams fromRpcValue(const shv::chainpack::RpcValue &rv);
};

enum class QxRecOp { Insert, Update, Delete, };

struct QxRecChng
{
	QString table;
	int64_t id;
	QVariant record;
	QxRecOp op;

	shv::chainpack::RpcValue toRpcValue() const;
};

class SqlApi : public QObject
{
	Q_OBJECT
public:
	static SqlApi* instance();

	Q_SIGNAL void recchng(const QxRecChng &chng);

	static RpcSqlResult exec(const SqlQueryAndParams &params);
	static RpcSqlResult query(const SqlQueryAndParams &params);
	static void transaction(const std::string &query, const shv::chainpack::RpcValue::List &params);
	static RpcSqlResult list(const std::string &table, const std::vector<std::string> &fields, std::optional<int64_t> ids_above, std::optional<int64_t> limit);
	static int64_t create(const std::string &table, const SqlRecord &record);
	static std::optional<SqlRecord> read(const std::string &table, int64_t id, const std::vector<std::string> &fields);
	static bool update(const std::string &table, int64_t id, const SqlRecord &record);
	static bool drop(const std::string &table, int64_t id);
private:
	explicit SqlApi(QObject *parent = nullptr);
};

}
