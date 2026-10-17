#ifndef QF_CORE_SQL_CONNECTION_H
#define QF_CORE_SQL_CONNECTION_H

#include "../core/coreglobal.h"

#include <QString>
#include <QMap>
#include <QSqlDatabase>
#include <QSqlIndex>
#include <QSharedData>
#include <QStringList>

class QProcessEnvironment;

namespace qf {
namespace core {
namespace sql {

class QFCORE_DECL_EXPORT Connection : public QSqlDatabase
{
private:
	typedef QSqlDatabase Super;
public:
	/**
	 * @brief Connection
	 * Creates an empty, invalid Connection object.
	 */
	Connection();
	explicit Connection(const QSqlDatabase& qdb);
	Connection& operator=(const QSqlDatabase& o);
public:
	typedef QMap<QString, QString> ConnectionOptions;
	struct QFCORE_DECL_EXPORT IndexInfo {
		QString name;
		bool unique;
		bool primary;
		QStringList fields;

		IndexInfo() : unique(false), primary(false) {}
		bool operator==(const IndexInfo &) const = default;
	};
	typedef QList<IndexInfo> IndexList;
private:
	bool open(const QString& user, const QString& password);
public:
	bool open();
	void close();

	static Connection forName(const QString &connection_name = QString());
	static Connection defaultConnection() {return forName(QString());}

	int connectionId();

	//! @return list of fields in table or view
	QStringList fields(const QString& tbl_name) const;

	/// @return list of available tables
	QStringList tables(const QString& dbname = QString(), QSql::TableType type = QSql::Tables) const;
	bool tableExists(const QString &table_name);

	/// @return list of indexes for table \a tbl_name .
	IndexList indexes(const QString& tbl_name) const;

	/// @return list of available databases
	QStringList databases() const;

	/// @return list of available schemas in current connection
	QStringList schemas() const;

	QSqlIndex primaryIndex(const QString& table_id) const;
	QStringList primaryIndexFieldNames(const QString &table_id);
	QString serialFieldName(const QString &table_id);

	QSqlRecord record(const QString & table_id) const;

	/// @return string unique per user,database_name,host,driver
	QString signature() const;
	//! Returns human readable textual information about current connection.
	QString info(int verbosity = 1) const;
	bool isOpen() const;
	QString errorString() const;

	static int defaultPort(const QString &driver_name);
public:
	bool createSchema(const QString &schema_name);
	QString currentSchema() const;
	bool setCurrentSchema(const QString &schema_name);

	QString createSchemaSqlCommand(const QString &schema_name, bool include_data);

	//! retrieves CREATE TABLE ... Sql script for \a tblname.
	QString createTableSqlCommand(const QString &tblname);

	//! retrieves INSERT INTO ... Sql script for \a tblname.
	QString dumpTableSqlCommand(const QString &tblname);

	QStringList serverVersion() const;
	QString fullTableNameToQtDriverTableName(const QString &full_table_name) const;

	static QString escapeJsonForSql(const QString &json_string);
private:
	QString invokeProcess(const QString &prog_name, const QStringList params, const QProcessEnvironment &env);
	QString dumpSqlSchema_psql(const QString &schema_name, bool dump_data);
	QString dumpSqlTable_psql(const QString &tblname, bool dump_data);
	//! take CREATE TABLE ... and parse fields definitions from it.
	static QStringList fieldDefsFromCreateTableCommand(const QString &cmd);

	//! convert n to form .dbname.tblname.fldname
	QString normalizeFieldName(const QString &n) const;
	//! convert n to form .dbname.tblname (.schemaname.tblname for PSQL)
	QString normalizeTableName(const QString &n) const;
	//! if \a n is empty return name of default database/schema for active connection.
	QString normalizeDbName(const QString &n) const;
};

}}}

#endif // QF_CORE_SQL_CONNECTION_H
