#include "classesplugin.h"
#include "classdocument.h"
#include "classeswidget.h"

#include "importcoursedef.h"

#include <quickevent/core/codedef.h>
#include <quickevent/core/si/punchrecord.h>

#include <qf/gui/framework/mainwindow.h>
#include <qf/gui/dialogs/messagebox.h>
#include <qf/gui/action.h>

#include <qf/core/log.h>
#include <qf/core/assert.h>
#include <qf/core/exception.h>
#include <qf/core/sql/query.h>
#include <qf/core/sql/querybuilder.h>
#include <qf/core/sql/transaction.h>
#include <plugins/Event/src/eventplugin.h>
#include "partwidget.h"

#include <QQmlEngine>

namespace qff = qf::gui::framework;
//namespace qfd = qf::gui::dialogs;
namespace qfs = qf::core::sql;
using ::PartWidget;
using qff::getPlugin;
using Event::EventPlugin;

namespace Classes {

void ClassDef::load(int class_id, int stage_id, bool is_relays)
{
	qf::core::sql::QueryBuilder qb;
	qb.select2("classdefs", "startTimeMin, lastStartTimeMin, startIntervalMin, vacantsBefore, vacantEvery, vacantsAfter")
			.from("classdefs")
			.where("classId=" QF_IARG(class_id));
	if(!is_relays)
		qb.where("stageId=" QF_IARG(stage_id));
	qfs::Query q(qfs::Connection::forName());
	//qfInfo() << qb.toString();
	q.exec(qb.toString(), qf::core::Exception::Throw);
	if(q.next()) {
		classInterval = q.value("startIntervalMin").toInt() * 60 * 1000;
		classStartFirst = q.value("startTimeMin").toInt() * 60 * 1000;
		classStartLast = q.value("lastStartTimeMin").toInt() * 60 * 1000;
	}
	else {
		classInterval = 0;
		classStartFirst = 0;
		classStartLast = 0;
	}
}

ClassesPlugin::ClassesPlugin(QObject *parent)
	: Super("Classes", parent)
{
	connect(this, &ClassesPlugin::installed, this, &ClassesPlugin::onInstalled);
}

void ClassesPlugin::onInstalled()
{
	m_partWidget = qff::initPluginWidget<ClassesWidget, PartWidget>(tr("Classes"), featureId());
}

QObject *ClassesPlugin::createClassDocument(QObject *parent)
{
	auto *ret = new ClassDocument(parent);
	return ret;
}

void ClassesPlugin::createClass(const QString &class_name)
{
	ClassDocument doc;
	doc.loadForInsert();
	doc.setValue(QStringLiteral("classes.name"), class_name);
	doc.save();
	//transaction.commit();
}

void ClassesPlugin::dropClass(int class_id)
{
	QF_ASSERT_EX(class_id > 0, "Bad classes.id value.");
	//qfInfo() << "Dropping class id:" << class_id;
	ClassDocument doc;
	doc.load(class_id, ClassDocument::RecordEditMode::ModeDelete);
	doc.drop();
}

void ClassesPlugin::createCourses(int stage_id, const QList<ImportCourseDef> &courses, const QList<quickevent::core::CodeDef> &codes, bool delete_current)
{
	qfLogFuncFrame();
	try {
		bool is_relays = getPlugin<EventPlugin>()->eventConfig().isRelays();
		qf::core::sql::Transaction transaction(qf::core::sql::Connection::forName());
		qf::core::sql::Query q;
		if (delete_current)
			deleteCourses(stage_id);

		QMap<int, quickevent::core::CodeDef> code_defs;
		if(codes.empty()) {
			for(const ImportCourseDef &icd : courses) {
				for(const auto &vc : icd.codes()) {
					quickevent::core::CodeDef cd(vc.toString());
					int key = cd.code();
					qfDebug() << "Creating code def from:" << vc.toString() << "code:" << key << "def:" << cd.toString();
					if(key == 0)
						qfWarning() << "Invalid code definition:" << vc;
					else
						code_defs[key] = cd;
				}
			}
		}
		else {
			for(const quickevent::core::CodeDef &cd : codes) {
				int key = cd.code();
				code_defs[key] = cd;
			}
		}

		QMap<QString, int> course_ids;
		QMap<int, QList<int> > course_codes;
		{
			// if classes are not imported from Oris, import also classes
			q.exec("SELECT COUNT(*) FROM classes", qf::core::Exception::Throw);
			if(q.next() && q.value(0).toInt() == 0) {
				QSet<QString> class_names;
				for(const ImportCourseDef &cd : courses) {
					for(const auto &class_name : cd.classes())
						class_names << class_name;
				}
				ClassDocument doc;
				for(const auto &class_name : class_names) {
					qfInfo() << "inserting class" << class_name;
					doc.loadForInsert();
					doc.setValue("name", class_name);
					doc.save();
				}
			}
		}
		QMap<QString, int> class_ids;
		{
			q.exec("SELECT id, name FROM classes");
			while(q.next()) {
				class_ids[q.value("name").toString()] = q.value("id").toInt();
			}
		}
		for(const ImportCourseDef &cd : courses) {
			int course_id = 0;
			{
				qfInfo() << "inserting course" << cd.name() << "stage:" << stage_id << "classes:" << cd.classes().join(',');
				QString qs = "INSERT INTO courses (name, length, climb, note) VALUES (:name, :length, :climb, :note)";
				q.prepare(qs, qf::core::Exception::Throw);
				q.bindValue(":name", cd.name());
				q.bindValue(":length", cd.lenght());
				q.bindValue(":climb", cd.climb());
				q.bindValue(":note", QString("E%1 ").arg(stage_id) + cd.classes().join(','));
				q.exec(qf::core::Exception::Throw);
				course_id = q.lastInsertId().toInt();
			}
			if(!is_relays) {
				QString qs = "UPDATE classdefs SET courseId=:courseId WHERE classId=:classId AND stageId=:stageId";
				q.prepare(qs, qf::core::Exception::Throw);
				for(const auto &class_name : cd.classes()) {
					int class_id = class_ids.value(class_name);
					if(class_id > 0) {
						qfInfo() << "\t" << "updating classdefs for" << class_name << "stage:" << stage_id;
						q.bindValue(":classId", class_id);
						q.bindValue(":courseId", course_id);
						q.bindValue(":stageId", stage_id);
						q.exec(qf::core::Exception::Throw);
					}
					else {
						qfError() << class_name << "not found in defined classes";
					}
				}
			}
			course_ids[cd.name()] = course_id;
			for(const auto &v : cd.codes()) {
				auto code = v.toInt();
				if(code <= 0)
					QF_EXCEPTION("Invalid code: " + v.toString());
				if(codes.isEmpty()) {
					/// guess code definition from courses
					quickevent::core::CodeDef cd;
					cd.setCode(code);
				}
				else {
					if(!code_defs.contains(code)) {
						QF_EXCEPTION("Defined controls should contain code: " + QString::number(code));
					}
				}
				course_codes[course_id] << code;
			}
		}
		if(is_relays) {
			/// guess first relay number and number of legs in each class
			struct RelNoLegs {
				int relayNo = std::numeric_limits<int>::max();
				int legCnt = 0;
				bool isValid() const {return relayNo < std::numeric_limits<int>::max() && legCnt > 0;}
			};
			QMap<QString, RelNoLegs> relnolegs;
			for(const ImportCourseDef &cd : courses) {
				if(cd.classes().count() == 1) {
					QString name = cd.name();
					int num = name.section('.', 0, 0).toInt();
					int leg = name.section('.', 1, 1).toInt();
					if(num > 0 && leg > 0) {
						RelNoLegs &rnl = relnolegs[cd.classes().value(0)];
						rnl.relayNo = qMin(rnl.relayNo, num);
						rnl.legCnt = qMax(rnl.legCnt, leg);
					}
					else {
						qfWarning() << "Cannot deduce relay number and leg for course:" << cd.name() << "classes:" << cd.classes().join(',');
					}
				}
				else {
					qfWarning() << "Cannot deduce class name for course:" << cd.name() << "classes:" << cd.classes().join(',');
				}
			}
			QString qs = "UPDATE classdefs SET relayStartNumber=:relayStartNumber, relayLegCount=:relayLegCount WHERE classId=:classId AND stageId=:stageId";
			q.prepare(qs, qf::core::Exception::Throw);
			QMapIterator<QString, RelNoLegs> it(relnolegs);
			while(it.hasNext()) {
				it.next();
				const QString class_name = it.key();
				if(it.value().isValid()) {
					int class_id = class_ids.value(class_name);
					if(class_id > 0) {
						qfInfo() << "\t" << "updating classdefs for" << class_name << "stage:" << stage_id
								 << "relayStartNumber:" << it.value().relayNo << "relayLegCount:" << it.value().legCnt;
						q.bindValue(":relayStartNumber", it.value().relayNo);
						q.bindValue(":relayLegCount", it.value().legCnt);
						q.bindValue(":classId", class_id);
						q.bindValue(":stageId", stage_id);
						q.exec(qf::core::Exception::Throw);
					}
					else {
						qfError() << class_name << "not found in defined classes";
					}
				}
			}
		}
		QMap<int, int> code_to_id;
		{
			QString qs = "INSERT INTO codes (code, note, latitude, longitude) VALUES (:code, :note, :latitude, :longitude)";
			q.prepare(qs, qf::core::Exception::Throw);
			QMapIterator<int, quickevent::core::CodeDef > it(code_defs);
			while(it.hasNext()) {
				it.next();
				quickevent::core::CodeDef cd = it.value();
				qfDebug() << "inserting code" << cd.toString();
				//q.bindValue(":type", cd.type().isEmpty()? QString(""): cd.type()); /// save empty not null string
				//q.bindValue(":type", cd.type());
				q.bindValue(":code", cd.code());
				q.bindValue(":note", QString("E%1").arg(stage_id));
				q.bindValue(":latitude", cd.latitude());
				q.bindValue(":longitude", cd.longitude());
				q.exec(qf::core::Exception::Throw);
				//QString key = cd.type() + QString::number(cd.code());
				code_to_id[it.key()] = q.lastInsertId().toInt();
			}
		}
		{
			QString qs = "INSERT INTO coursecodes (courseId, position, codeId) VALUES (:courseId, :position, :codeId)";
			q.prepare(qs, qf::core::Exception::Throw);
			QMapIterator<int, QList<int> > it(course_codes);
			while(it.hasNext()) {
				it.next();
				int pos = 0;
				for(int code : it.value()) {
					int code_id = code_to_id.value(code);
					quickevent::core::CodeDef cd = code_defs.value(code);
					if(code_id > 0) {
						qfDebug() << "courseId" << it.key() << "-> code:" << code << "codeId:" << code_id;
						/// keep start control position == 0 to have first control on position == 1
						if(code >= quickevent::core::CodeDef::PUNCH_CODE_MIN)
							pos++;
						q.bindValue(":courseId", it.key());
						q.bindValue(":position", pos);
						q.bindValue(":codeId", code_id);
						q.exec(qf::core::Exception::Throw);
					}
					else {
						QF_EXCEPTION(tr("Cannot find id for code: %1").arg(code));
					}
				}
			}
		}
		transaction.commit();
	}
	catch (const qf::core::Exception &e) {
		qf::gui::framework::MainWindow *fwk = qf::gui::framework::MainWindow::frameWork();
		qf::gui::dialogs::MessageBox::showException(fwk, e);
	}
}

void ClassesPlugin::deleteCourses(int stage_id)
{
	qf::core::sql::Query q;
	q.exec("UPDATE classdefs SET courseId=NULL WHERE stageId=" QF_IARG(stage_id), qf::core::Exception::Throw);
	gcCourses();
}

void ClassesPlugin::gcCourses()
{
	qf::core::sql::Query q;
	q.exec("DELETE FROM coursecodes WHERE id IN ("
		"SELECT coursecodes.id FROM coursecodes"
		   " LEFT JOIN courses ON coursecodes.courseId=courses.id"
		   " LEFT JOIN classdefs ON classdefs.courseId=courses.id"
		   " WHERE classdefs.Id IS NULL"
		")", qf::core::Exception::Throw);
	q.exec("DELETE FROM courses WHERE id IN ("
		"SELECT courses.id FROM courses LEFT JOIN classdefs ON classdefs.courseId=courses.id WHERE classdefs.Id IS NULL"
		")", qf::core::Exception::Throw);
	q.exec("DELETE FROM codes WHERE id IN ("
		"SELECT codes.id FROM codes LEFT JOIN coursecodes ON coursecodes.codeId=codes.id WHERE coursecodes.Id IS NULL"
		   ")", qf::core::Exception::Throw);
}

}
