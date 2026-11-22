#include "dbschema.h"
#include "eventplugin.h"
#include "plugins/Event/src/appdbconfig.h"

#include <optional>
#include <qf/core/assert.h>
#include <qf/core/log.h>
#include <qf/core/sql/connection.h>
#include <qf/core/sql/transaction.h>

#include <QDateTime>
#include <QFile>
#include <QRegularExpression>
#include <QSqlError>
#include <QSqlField>
#include <QSqlQuery>
#include <QSqlRecord>

namespace {

using   qf::core::sql::Connection;

struct Migration
{
	int version; // DB version reached by this migration
	bool (*apply)(const Connection &conn);
};

/// executes the ';' delimited SQL commands written for the driver of conn, stops on first error
/// commands are split on every ';', so it cannot be used inside of a command (string literal, trigger body)
bool exec_sql(const Connection &conn, const QStringList &psql_commands, std::optional<QStringList> sqlite_commands)
{
	const auto is_sqlite = conn.driverName().endsWith(QLatin1String("SQLITE"));
	const auto &commands = is_sqlite? sqlite_commands.value_or(psql_commands): psql_commands;
	QSqlQuery q(conn);
	for (const auto &sql_command : commands) {
		if (!q.exec(sql_command)) {
			qfError() << "Migration SQL error:" << q.lastError().text() << "command:" << sql_command;
			return false;
		}
		q.finish();
	}
	return true;
}

bool has_column(const Connection &conn, const QString &table, const QString &column)
{
	QSqlQuery q(conn);
	// Connection::record() is cached, ask the database directly
	if (!q.exec(QStringLiteral("SELECT * FROM %1 LIMIT 0").arg(table))) {
		qfError() << "Cannot read table" << table << ":" << q.lastError().text();
		return false;
	}
	return q.record().contains(column);
}

bool migrate_31000(const Connection &conn)
{
	// each step checks the schema itself, some events may already have part of the changes
	if (!has_column(conn, "qxchanges", "paid")) {
		qfInfo() << "Adding paid column to qxchanges";
		auto ok = exec_sql(conn,
				QStringList {
				"ALTER TABLE qxchanges ADD COLUMN paid boolean NOT NULL DEFAULT false",
				},
				QStringList {
				"ALTER TABLE qxchanges ADD COLUMN paid boolean NOT NULL DEFAULT 0",
				});
		if (!ok) {
			return false;
		}
	}
	if (has_column(conn, "config", "ckey")) {
		// config key becomes record id, then config can be accessed by qxsql CRUD API like other tables
		qfInfo() << "Renaming config.ckey to config.id";
		auto ok = exec_sql(conn,
				QStringList {
				"ALTER TABLE config RENAME COLUMN ckey TO id",
				},
				std::nullopt);
		if (!ok) {
			return false;
		}
	}
	return true;
}

// oldest event data version the migrations below can start from
constexpr int MIN_MIGRATABLE_VERSION = 30900;

// migration stack, append new migrations to the end
constexpr Migration MIGRATIONS[] = {
	{.version=31000, .apply=migrate_31000},
};

constexpr bool migrations_are_ascending()
{
	int last_version = MIN_MIGRATABLE_VERSION;
	for (const auto &migration : MIGRATIONS) {
		if (migration.version <= last_version) {
			return false;
		}
		last_version = migration.version;
	}
	return true;
}
static_assert(migrations_are_ascending(), "MIGRATIONS must be sorted by ascending version");

const Migration* next_migration(int db_version)
{
	for (const auto &migration : MIGRATIONS) {
		if (migration.version > db_version) {
			return &migration;
		}
	}
	return nullptr;
}

} // namespace

DbSchema::DbSchema(Event::EventPlugin *event_plugin)
	: QObject(event_plugin)
{
	QF_ASSERT_EX(event_plugin != nullptr, "Bad plugin!");
}

QString DbSchema::loadCreateDbSqlFile(const QString &driver_name)
{
	auto dbtype = driver_name.endsWith("PSQL", Qt::CaseInsensitive)? "psql": "sqlite";
	auto file_name = QStringLiteral(":/quickevent/Event/sql/create_db_%1.sql").arg(dbtype);
	QFile f(file_name);
	if (!f.open(QFile::ReadOnly | QIODevice::Text)) {
		qfError() << "Cannot open file:" << f.fileName() << "for reading";
		return {};
	}
	return QString::fromUtf8(f.readAll());
}

QStringList DbSchema::loadCreateDbSqlScript(const CreateDbSqlScriptOptions &create_options)
{
	auto sql = loadCreateDbSqlFile(create_options.driverName());
	if (sql.isEmpty()) {
		return {};
	}
	QVariantMap replacements;
	replacements["eventId"] = create_options.schemaName();
	replacements["minDbVersion"] = Event::EventPlugin::dbVersion();
	sql = qf::core::Utils::replaceCaptions(sql, replacements);
	auto lines = sql.split(";\n");
	return lines;
}

const QStringList& DbSchema::tableNames()
{
	if (m_tableNames.isEmpty()) {
		static const QRegularExpression rx(QStringLiteral(R"(^CREATE TABLE (\w+))"), QRegularExpression::MultilineOption);
		const auto sql = loadCreateDbSqlFile(QStringLiteral("SQLITE"));
		auto it = rx.globalMatch(sql);
		while (it.hasNext()) {
			m_tableNames << it.next().captured(1);
		}
		QF_CHECK(!m_tableNames.isEmpty(), "No tables found in create DB script!");
	}
	return m_tableNames;
}

QSqlRecord DbSchema::sqlRecord(const qf::core::sql::Connection &conn, const QString &table_name)
{
	QSqlRecord ret = conn.record(table_name);
	if (conn.driverName().endsWith(QLatin1String("SQLITE"))) {
		// SQLite has no date-time type, timestamp fields are reported as QString
		QSqlQuery q(conn);
		q.setForwardOnly(true);
		if (!q.exec(QStringLiteral("PRAGMA main.table_xinfo('%1')").arg(table_name))) {
			qfWarning() << "Cannot get table info for:" << table_name;
			return ret;
		}
		while (q.next()) {
			if (q.value(2).toString().compare(QLatin1String("timestamp"), Qt::CaseInsensitive) != 0) {
				continue;
			}
			auto ix = ret.indexOf(q.value(1).toString());
			if (ix >= 0) {
				QSqlField fld = ret.field(ix);
				fld.setMetaType(QMetaType::fromType<QDateTime>());
				ret.replace(ix, fld);
			}
		}
	}
	return ret;
}

bool DbSchema::canMigrate(int db_version)
{
	return db_version >= MIN_MIGRATABLE_VERSION && db_version < Event::EventPlugin::dbVersion();
}

int DbSchema::baseDbVersion()
{
	return MIN_MIGRATABLE_VERSION;
}

bool DbSchema::migrate(const qf::core::sql::Connection &conn)
{
	return migrate(conn, Event::EventPlugin::dbVersion());
}

bool DbSchema::migrate(const qf::core::sql::Connection &conn, int target_version)
{
	auto current_version = Event::AppDbConfig::loadDbVersion(conn);
	if (!canMigrate(current_version) || target_version <= current_version || target_version > Event::EventPlugin::dbVersion()) {
		qfError() << "Cannot migrate event data from version" << current_version << "to" << target_version;
		return false;
	}
	while (current_version < target_version) {
		const auto *migration = next_migration(current_version);
		// app versions without schema change have no migration, only the version number is raised
		if (migration && migration->version > target_version) {
			migration = nullptr;
		}
		auto next_version = migration? migration->version: target_version;
		{
			qf::core::sql::Transaction transaction(conn);
			QSqlQuery q(conn);
			// claim the step first, only the client which still sees the old version succeeds,
			// concurrent client waits here and then finds nothing to do
			// key column is renamed by migrate_31000
			auto config_id_key = current_version < 31000? "ckey": "id";
			const auto claim = QStringLiteral("UPDATE config SET cvalue='%1' WHERE %2='db.version' AND cvalue='%3'")
					.arg(next_version).arg(config_id_key).arg(current_version);
			if (!q.exec(claim)) {
				qfError() << "Cannot update event data version:" << q.lastError().text();
				return false;
			}
			if (q.numRowsAffected() > 0) {
				if (migration && !migration->apply(conn)) {
					return false;
				}
				transaction.commit();
				qfInfo() << "=== Event database migrated from" << current_version << "to" << next_version;
			}
			else {
				transaction.rollback();
			}
		}
		current_version = next_version;
	}
	return true;
}
