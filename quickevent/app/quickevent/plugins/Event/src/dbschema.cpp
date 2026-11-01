#include "dbschema.h"
#include "eventplugin.h"

#include <qf/core/assert.h>
#include <qf/core/log.h>
#include <qf/core/sql/connection.h>

#include <QDateTime>
#include <QFile>
#include <QRegularExpression>
#include <QSqlField>
#include <QSqlQuery>
#include <QSqlRecord>

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
