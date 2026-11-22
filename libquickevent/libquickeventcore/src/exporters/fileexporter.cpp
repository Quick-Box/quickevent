#include "fileexporter.h"

#include <qf/core/sql/query.h>
#include <qf/core/log.h>
#include <qf/core/utils/htmlutils.h>
#include <qf/core/collator.h>

#include <QDir>



namespace quickevent::core::exporters {

FileExporter::FileExporter(QObject *parent)
	: QObject(parent)
{
}

QVariantMap FileExporter::eventInfo()
{
	if(m_eventInfo.isEmpty()) {
		auto q = execSql("SELECT id, cvalue FROM config WHERE id LIKE 'event.%'");
		while(q.next()) {
			m_eventInfo[q.value(0).toString().mid(6)] = q.value(1);
		}
	}
	return m_eventInfo;
}

qf::core::sql::Query FileExporter::execSql(const QString &query_str)
{
	QString qs = query_str;
	qf::core::sql::Query q(sqlConnection());
	if(!q.exec(qs)) {
		QSqlError err = q.lastError();
		qfError() << "SQL ERROR:" << err.text();
	}
	return q;
}

QString FileExporter::normalizeClassName(const QString class_name)
{
	QString ret = class_name;
	ret.replace(' ', '-');
	return QString::fromUtf8(qf::core::Collator::toAscii7(QLocale::Czech, ret, true));
}

void FileExporter::prepareExport()
{
	QDir export_dir(outDir());
	if(!export_dir.exists()) {
		qfInfo() << "creating export dir:" << outDir();
		if(!QDir().mkpath(outDir())) {
			qfError() << "Cannot create export dir:" << outDir();
			return;
		}
		export_dir = QDir(outDir());
		if(!export_dir.exists()) {
			qfError() << "Author even doesn't know, how to use QDir API.";
			return;
		}
	}
	QVariantMap event_info = eventInfo();
	int curr_stage = currentStage();
	if(curr_stage == 0) {
		curr_stage = event_info.value("currentStageId").toInt();
		if(curr_stage == 0)
			curr_stage = 1;
		setCurrentStage(curr_stage);
	}
}

}
