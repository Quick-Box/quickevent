#pragma once

#include <qf/core/utils.h>

#include <QObject>
#include <QVariantMap>

namespace Event {
class EventPlugin;
}
namespace qf::core::sql {
class Connection;
}

class QSqlRecord;

class DbSchema : public QObject
{
public:
	class CreateDbSqlScriptOptions : public QVariantMap
	{
	public:
		CreateDbSqlScriptOptions(const QVariantMap &o = QVariantMap()) : QVariantMap(o) {}

		QF_VARIANTMAP_FIELD(QString, s, setS, chemaName)
		QF_VARIANTMAP_FIELD(QString, d, setD, riverName)
	};
public:
	DbSchema(Event::EventPlugin *event_plugin);

	QStringList loadCreateDbSqlScript(const CreateDbSqlScriptOptions &create_options);
	/// event tables in order of creation, so referenced tables are listed before referencing ones
	const QStringList& tableNames();
	/// table record as it exists in database of connection
	QSqlRecord sqlRecord(const qf::core::sql::Connection &conn, const QString &table_name);
private:
	static QString loadCreateDbSqlFile(const QString &driver_name);
private:
	QStringList m_tableNames;
};
