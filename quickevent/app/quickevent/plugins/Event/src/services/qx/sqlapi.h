#pragma once

#include <shv/chainpack/rpcvalue.h>
// #include <qf/core/sql/qxrecchng.h>
#include <qf/gui/model/sqltablemodel.h>

#include <QObject>
#include <QVariantMap>

namespace qf::core::sql { struct QxRecChng; }

namespace Event::services::qx {

struct DbField
{
	QString name;

	//explicit DbField(const QJsonObject &jo = QJsonObject()) : Super(jo) {}
	shv::chainpack::RpcValue toRpcValue() const;
	// QVariant toVariant() const;
	static DbField fromRpcValue(const shv::chainpack::RpcValue &rv);
	// static DbField fromVariant(const QVariant &v);
};

struct ExecResult
{
	int numRowsAffected = 0;
	std::optional<int> lastInsertId = 0;

	shv::chainpack::RpcValue toRpcValue() const;
	static ExecResult fromRpcValue(const shv::chainpack::RpcValue &rv);
};

struct QueryResult
{
	std::vector<DbField> fields;
	using Row = QVariantList;
	QList<Row> rows;

	std::optional<qsizetype> columnIndex(const std::string &name) const;
	QVariant value(qsizetype row, qsizetype col) const;
	QVariant value(qsizetype row, const std::string &name) const;
	void setValue(qsizetype row, qsizetype col, const QVariant &val);
	void setValue(qsizetype row, const std::string &name, const QVariant &val);

	shv::chainpack::RpcValue toRpcValue() const;
	static QueryResult fromRpcValue(const shv::chainpack::RpcValue &rv);
	shv::chainpack::RpcValue::List toRecordList() const;
};

using Record = QVariantMap;

struct SqlQueryAndParams
{
	QString query;
	Record params;

	static SqlQueryAndParams fromRpcValue(const shv::chainpack::RpcValue &rv);
};

shv::chainpack::RpcValue qxRecChngToRpcValue(const qf::core::sql::QxRecChng &chng);


class SqlApi : public QObject
{
	Q_OBJECT
public:
	explicit SqlApi(QObject *parent = nullptr);

	// Q_SIGNAL void recchng(const qf::core::sql::QxRecChng &chng);
	// void emitRecChng(const qf::core::sql::QxRecChng &chng);

	ExecResult exec(const SqlQueryAndParams &params);
	QueryResult query(const SqlQueryAndParams &params);
	void transaction(const std::string &query, const shv::chainpack::RpcValue::List &params);
	QueryResult list(const std::string &table, const std::vector<std::string> &fields, std::optional<int64_t> ids_above, std::optional<int64_t> limit);
	int64_t create(const std::string &table, const shv::chainpack::RpcValue::Map &record);
	std::optional<Record> read(const std::string &table, int64_t id, const std::vector<std::string> &fields);
	bool update(const std::string &table, int64_t id, const shv::chainpack::RpcValue::Map &record);
	bool drop(const std::string &table, int64_t id);
private:
};

}
