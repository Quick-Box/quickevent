#include "application.h"
#include "appclioptions.h"

#include <qf/core/log.h>
#include <qf/core/sql/connection.h>
#include <qf/core/sql/query.h>

#include <QSettings>
#include <QStringList>
#include <QStringBuilder>
#include <QSqlError>
#include <QSqlRecord>
#include <QSqlDatabase>

Application::Application(int &argc, char **argv, AppCliOptions *cli_opts)
	: Super(argc, argv)
	, m_cliOptions(cli_opts)
{
}

Application *Application::instance()
{
	Application *ret = qobject_cast<Application*>(Super::instance());
	if(!ret)
		qFatal("Invalid Application instance");
	return ret;
}

qf::core::sql::Connection Application::sqlConnetion()
{
	qf::core::sql::Connection db = qf::core::sql::Connection::forName();
	if(!db.isValid()) {
		db = QSqlDatabase::addDatabase(cliOptions()->sqlDriver());
		db.setHostName(cliOptions()->host());
		db.setPort(cliOptions()->port());
		db.setDatabaseName(cliOptions()->database());
		db.setUserName(cliOptions()->user());
		db.setPassword(cliOptions()->password());
		qfInfo() << "connecting to database:"
				 << db.databaseName()
				 << "at:" << (db.userName() + '@' + db.hostName() + ':' + QString::number(db.port()))
				 << "driver:" << db.driverName()
				 << "...";// << db.password();
		bool ok = db.open();
		if(!ok) {
			qfError() << "ERROR open database:" << db.lastError().text();
		}
		else {
			if(!cliOptions()->sqlDriver().endsWith(QLatin1String("SQLITE"))) {
				QString event_name = cliOptions()->eventName();
				if(event_name.isEmpty()) {
					qfError() << "Event name is empty!";
				}
				else {
					qfInfo() << "\tSetting current schema to" << cliOptions()->eventName();
					db.setCurrentSchema(cliOptions()->eventName());
					if(db.currentSchema() != cliOptions()->eventName()) {
						qfError() << "ERROR open event:" << cliOptions()->eventName();
					}
				}
			}
			if(ok) {
				qfInfo() << "\tOK";
				setSqlConnected(true);
			}
		}
	}
	return db;
}

qf::core::sql::Query Application::execSql(const QString &query_str)
{
	QString qs = query_str;
	qf::core::sql::Query q(sqlConnetion());
	if(!q.exec(qs)) {
		QSqlError err = q.lastError();
		qfError() << "SQL ERROR:" << err.text();
		//qfError() << ("QUERY: "%q.lastQuery());
		::exit(-1);
	}
	return q;
}
/*
QVariantMap Application::sqlRecordToMap(const QSqlRecord &rec)
{
	QVariantMap ret;
	for(int i=0; i<rec.count(); i++) {
		QString fld_name = rec.fieldName(i).section('.', -1, -1).toLower();
		QVariant v = rec.value(i);
		//if(v.type() == QVariant::String) v = recodeSqlString(v.toString());
		ret[fld_name] = v;
		//qDebug() << fld_name << "->" << v.toString();
	}
	return ret;
}
*/
QVariantMap Application::eventInfo()
{
	static QVariantMap info;
	if(info.isEmpty()) {
		QSqlQuery q = execSql("SELECT id, cvalue FROM config WHERE id LIKE 'event.%'");
		while(q.next())
			info[q.value(0).toString().mid(6)] = q.value(1);
		info["profile"] = profile();
		//set info about relay
		info["isRelay"] = (info["disciplineId"] == 5 || info["disciplineId"] == 6);
	}
	return info;
}

QString Application::profile()
{
	return cliOptions()->profile();
}


