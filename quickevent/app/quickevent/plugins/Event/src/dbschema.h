#pragma once

#include <qf/core/utils.h>

#include <QList>
#include <QVariantMap>

namespace Event {
class EventPlugin;
}

class QObject;
class QSqlRecord;
class QQmlComponent;
class QQmlEngine;

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

	QStringList createDbSqlScriptQml(const CreateDbSqlScriptOptions &create_options);
	QStringList loadCreateDbSqlScript(const CreateDbSqlScriptOptions &create_options);
	QList<QObject*> tables();
	QObject* table(const QString &table_name);
	QSqlRecord sqlRecord(QObject *table, bool lowercase_field_names = false);
private:
	QQmlEngine *qmlEngine();
	QObject* dbSchemaRoot();
private:
	Event::EventPlugin *m_eventPlugin;
	QQmlComponent *m_dbschemaComponent = nullptr;
	QObject *m_dbSchemaRoot = nullptr;
	QQmlEngine *m_qmlEngine = nullptr;
};

