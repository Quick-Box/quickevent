#include "classesplugin.h"
#include "classeswidget.h"
#include "ui_classeswidget.h"

#include "importcoursedef.h"
#include "editcodeswidget.h"
#include "editcourseswidget.h"
#include "drawing/drawingganttwidget.h"

#include <quickevent/core/si/punchrecord.h>
#include <quickevent/core/codedef.h>

#include <qf/gui/action.h>
#include <qf/gui/menubar.h>
#include <qf/gui/toolbar.h>
#include <qf/gui/framework/mainwindow.h>
#include <qf/gui/dialogs/dialog.h>
#include <qf/gui/dialogs/filedialog.h>
#include <qf/gui/dialogs/messagebox.h>

#include <qf/core/sql/query.h>
#include <qf/core/string.h>
#include <qf/core/utils.h>
#include <qf/core/collator.h>
#include <qf/gui/model/sqltablemodel.h>
#include <qf/core/sql/querybuilder.h>
#include <qf/core/sql/connection.h>
#include <qf/core/assert.h>
#include <plugins/Event/src/eventplugin.h>

#include <QDomDocument>
#include <QComboBox>
#include <QStyledItemDelegate>
#include <QTextStream>

#if QT_VERSION < QT_VERSION_CHECK(5, 14, 0)
static const auto SkipEmptyParts = QString::SkipEmptyParts;
#else
static const auto SkipEmptyParts = Qt::SkipEmptyParts;
#endif

namespace qfc = qf::core;
namespace qfw = qf::gui;
namespace qfd = qf::gui::dialogs;
namespace qfm = qf::gui::model;
namespace qfs = qf::core::sql;
using qf::gui::framework::getPlugin;
using Event::EventPlugin;
using Classes::ClassesPlugin;

class CourseItemDelegate : public QStyledItemDelegate
{
	Q_OBJECT
public:
	CourseItemDelegate(QObject *parent) : QStyledItemDelegate(parent) {}

	Q_SIGNAL void courseIdChanged();

	QWidget *createEditor(QWidget *parent, const QStyleOptionViewItem &option, const QModelIndex &index) const Q_DECL_OVERRIDE
	{
		Q_UNUSED(option)
		Q_UNUSED(index)
		auto *editor = new QComboBox(parent);
#if QT_VERSION_MAJOR >= 6
		QMultiMapIterator<QString, int> it(m_courseNameToId);
#else
		QMapIterator<QString, int> it(m_courseNameToId);
#endif
		while(it.hasNext()) {
			it.next();
			editor->addItem(it.key(), it.value());
		}
		return editor;
	}
	void setEditorData(QWidget *editor, const QModelIndex &index) const Q_DECL_OVERRIDE
	{
		auto *cbx = qobject_cast<QComboBox *>(editor);
		QF_ASSERT(cbx != nullptr, "Bad combo!", return);
		QString id = index.data(Qt::EditRole).toString();
		int ix = cbx->findData(id);
		cbx->setCurrentIndex(ix);
	}
	void setModelData(QWidget *editor, QAbstractItemModel *model, const QModelIndex &index) const Q_DECL_OVERRIDE
	{
		qfLogFuncFrame();
		auto *cbx = qobject_cast<QComboBox *>(editor);
		QF_ASSERT(cbx != nullptr, "Bad combo!", return);
		qfDebug() << "setting model data:" << cbx->currentText() << cbx->currentData();
		model->setData(index, cbx->currentData(), Qt::EditRole);
		emit const_cast<CourseItemDelegate*>(this)->courseIdChanged(); // NOLINT(cppcoreguidelines-pro-type-const-cast)
	}

	QString displayText(const QVariant &value, const QLocale &locale) const Q_DECL_OVERRIDE
	{
		Q_UNUSED(locale)
		return m_idToCourseName.value(value.toInt(), QStringLiteral("???"));
	}

	void setCourses(const QMap<int, QString> &courses)
	{
		m_idToCourseName = courses;
		m_courseNameToId.clear();
		QMapIterator<int, QString> it(m_idToCourseName);
		while(it.hasNext()) {
			it.next();
			m_courseNameToId.insert(it.value(), it.key());
		}
	}

private:
	QMap<int, QString> m_idToCourseName;
	QMultiMap<QString, int> m_courseNameToId;
};

class CourseCodesTableModel : public qfm::SqlTableModel
{
	Q_OBJECT
private:
	using Super = qfm::SqlTableModel;
public:
	enum Columns {
		col_position,
		col_control_type,
		col_code,
		col_altCode,
		col_outOfOrder,
		col_radio,
		col_longitude,
		col_latitude,
		col_COUNT
	};

	CourseCodesTableModel(QObject *parent) : Super(parent)
	{
		clearColumns(col_COUNT);
		setColumn(col_position, ColumnDefinition("coursecodes.position", tr("Pos")).setReadOnly(true));
		setColumn(col_control_type, ColumnDefinition("control_type", tr("Type", "control type")).setToolTip(tr("Control type")).setReadOnly(true));
		setColumn(col_code, ColumnDefinition("codes.code", tr("Code")).setReadOnly(true));
		setColumn(col_altCode, ColumnDefinition("codes.altCode", tr("Alt")).setToolTip(tr("Code alternative")));
		setColumn(col_outOfOrder, ColumnDefinition("codes.outOfOrder", tr("O")).setToolTip(tr("Out of order")));
		setColumn(col_radio, ColumnDefinition("codes.radio", tr("R")).setToolTip(tr("Radio")));
		setColumn(col_longitude, ColumnDefinition("codes.longitude", tr("Long")).setToolTip(tr("Longitude")));
		setColumn(col_latitude, ColumnDefinition("codes.latitude", tr("Lat")).setToolTip(tr("Latitude")));
	}

	// QAbstractItemModel interface
public:
	QVariant data(const QModelIndex &index, int role) const override
	{
		if(index.column() == col_control_type) {
			if(role == Qt::DisplayRole) {
				QModelIndex ix = index.sibling(index.row(), col_code);
				int code = ix.data().toInt();
				using CodeDef = quickevent::core::CodeDef;
				if(CodeDef::codeToType(code) != CodeDef::Type::Control)
					return CodeDef::codeToString(code);
			}
			return QVariant();
		}
		return Super::data(index, role);
	}
};

ClassesWidget::ClassesWidget(QWidget *parent) :
	Super(parent),
	ui(new Ui::ClassesWidget)
{
	ui->setupUi(this);
	ui->splitter->setPersistentSettingsId(ui->splitter->objectName());
	{
		ui->tblClasses->setPersistentSettingsId("tblClasses");
		ui->tblClasses->setInsertRowEnabled(true);
		ui->tblClasses->setCloneRowEnabled(false);
		ui->tblClasses->setRemoveRowEnabled(true);
		ui->tblClasses->setDirtyRowsMenuSectionEnabled(false);

		ui->tblClassesTB->setTableView(ui->tblClasses);
		auto *m = new qfm::SqlTableModel(this);
		//m->setObjectName("classes.classesModel");
		m->addColumn("id").setReadOnly(true);
		m->addColumn("classes.name", tr("Class"));
		m->addColumn("classdefs.drawLock", tr("DL")).setToolTip(tr("Locked for drawing"));
		m->addColumn("classdefs.startTimeMin", tr("Start"));
		m->addColumn("classdefs.startIntervalMin", tr("Interval"));
		m->addColumn("classdefs.vacantsBefore", tr("VB")).setToolTip(tr("Vacants before"));
		m->addColumn("classdefs.vacantEvery", tr("VE")).setToolTip(tr("Vacant every"));
		m->addColumn("classdefs.vacantsAfter", tr("VA")).setToolTip(tr("Vacants after"));
		m->addColumn("classdefs.lastStartTimeMin", tr("Last")).setToolTip(tr("Start time of last competitor in class."));
		m->addColumn("runsCount", tr("Count")).setToolTip(tr("Runners count"));
		m->addColumn("classdefs.mapCount", tr("Maps"));
		m->addColumn("classdefs.courseId", tr("Course")).setAlignment(Qt::AlignLeft);
		//m->addColumn("courses.name", tr("Course")).setReadOnly(true);
		m->addColumn("courses.length", tr("Length"));
		m->addColumn("courses.climb", tr("Climb"));

		m->addColumn("relaysCount", tr("Rel. count")).setToolTip(tr("Relays count"));
		m->addColumn("classdefs.relayStartNumber", tr("Rel. num")).setToolTip(tr("Relay start number"));
		m->addColumn("classdefs.relayLegCount", tr("Legs")).setToolTip(tr("Relay leg count"));

		ui->tblClasses->setTableModel(m);

		m_courseItemDelegate = new CourseItemDelegate(this);
		ui->tblClasses->setItemDelegateForColumn(m->columnIndex("classdefs.courseId"), m_courseItemDelegate);

		connect(m_courseItemDelegate, &CourseItemDelegate::courseIdChanged, ui->tblClasses, &qfw::TableView::reloadCurrentRow, Qt::QueuedConnection);

		m_classesModel = m;
	}
	{
		ui->tblCourseCodes->setPersistentSettingsId("tblCourseCodes");
		ui->tblCourseCodes->setEditRowsMenuSectionEnabled(false);
		ui->tblCourseCodes->setDirtyRowsMenuSectionEnabled(false);
		ui->tblCourseCodesTB->setTableView(ui->tblCourseCodes);
		qfm::SqlTableModel *m = new CourseCodesTableModel(this);
		ui->tblCourseCodes->setTableModel(m);
		m_courseCodesModel = m;
	}
	connect(ui->tblClasses, &qf::gui::TableView::currentRowChanged, this, &ClassesWidget::reloadCourseCodes);
	connect(ui->chkUseAllMaps, &QCheckBox::toggled, this, [this](bool checked) {
		auto evplugin = getPlugin<EventPlugin>();
		auto data = evplugin->stageData(selectedStageId());
		data.setUseAllMaps(checked);
		evplugin->setStageData(selectedStageId(), data);
	});
}

ClassesWidget::~ClassesWidget()
{
	delete ui;
}

int ClassesWidget::selectedStageId()
{
	return m_cbxStage->currentData().toInt();
}

void ClassesWidget::settleDownInPartWidget(::PartWidget *part_widget)
{
	connect(part_widget, &::PartWidget::resetPartRequest, this, &ClassesWidget::reset);
	connect(part_widget, &::PartWidget::reloadPartRequest, this, &ClassesWidget::reload);

	qfw::Action *a_edit = part_widget->menuBar()->actionForPath("edit", true);
	a_edit->setText(tr("&Edit"));
	{
		auto *a = new qfw::Action(tr("Cou&rses"), this);
		connect(a, &QAction::triggered, this, &ClassesWidget::edit_courses);
		a_edit->addActionInto(a);
	}
	{
		auto *a = new qfw::Action(tr("Co&des"), this);
		connect(a, &QAction::triggered, this, &ClassesWidget::edit_codes);
		a_edit->addActionInto(a);
	}
	{
		auto *a = new qfw::Action(tr("Classes &layout"));
		a->setShortcut(tr("Ctrl+L"));
		connect(a, &QAction::triggered, this, &ClassesWidget::edit_classes_layout);
		a_edit->addActionInto(a);
	}

	qfw::Action *a_import = part_widget->menuBar()->actionForPath("import", true);
	a_import->setText(tr("&Import"));
	{
		auto *a = new qfw::Action(tr("OCAD TXT"), this);
		connect(a, &QAction::triggered, this, &ClassesWidget::import_ocad_txt);
		a_import->addActionInto(a);
	}
	{
		auto *a = new qfw::Action(tr("OCAD v8"), this);
		connect(a, &QAction::triggered, this, &ClassesWidget::import_ocad_v8);
		a_import->addActionInto(a);
	}
	{
		auto *a = new qfw::Action(tr("OCAD IOF XML 2.0"), this);
		connect(a, &QAction::triggered, this, &ClassesWidget::import_ocad_iofxml_2);
		a_import->addActionInto(a);
	}
	{
		auto *a = new qfw::Action(tr("OCAD IOF XML 3.0"), this);
		connect(a, &QAction::triggered, this, &ClassesWidget::import_ocad_iofxml_3);
		a_import->addActionInto(a);
	}

	qfw::ToolBar *main_tb = part_widget->toolBar("main", true);
	//main_tb->addAction(m_actCommOpen);
	{
		auto *lbl = new QLabel(tr("Stage "));
		main_tb->addWidget(lbl);
	}
	{
		m_cbxStage = new QComboBox();
		main_tb->addWidget(m_cbxStage);
	}

}

void ClassesWidget::edit_courses()
{
	qf::gui::dialogs::Dialog dlg(QDialogButtonBox::Close, this);
	int stage_id = selectedStageId();
	auto *w = new EditCoursesWidget(stage_id);
	dlg.setCentralWidget(w);
	dlg.exec();
	reload();
}

void ClassesWidget::edit_codes()
{
	qf::gui::dialogs::Dialog dlg(QDialogButtonBox::Close, this);
	auto *w = new EditCodesWidget();
	dlg.setCentralWidget(w);
	//auto *bt_apply = dlg.buttonBox()->button(QDialogButtonBox::Apply);
	//connect(bt_apply, &QPushButton::clicked, this, &MainWindow::askUserToRestartAppServer);
	dlg.exec();
	reload();
}

void ClassesWidget::edit_classes_layout()
{
	qf::core::sql::Query q;
	q.exec("SELECT id FROM classdefs WHERE (startIntervalMin IS NULL OR startIntervalMin = 0) AND stageId = " + QString::number(selectedStageId()));
	if(q.next()) {
		qfd::MessageBox::showWarning(
			qfw::framework::MainWindow::frameWork(),
			tr("Classes without start interval won't be displayed.\n"
			   "Consider setting \"Interval\" column for all classes before continuing."));
	}
	auto *w = new drawing::DrawingGanttWidget;
	qf::gui::dialogs::Dialog dlg(this);
	//dlg.setButtons(QDialogButtonBox::Save);
	dlg.setCentralWidget(w);
	w->load(selectedStageId());
	dlg.exec();
	reload();
}

void ClassesWidget::reset()
{
	{
		m_cbxStage->blockSignals(true);
		m_cbxStage->clear();
		for(int i=0; i < getPlugin<EventPlugin>()->stageCount(); i++)
			m_cbxStage->addItem(tr("E%1").arg(i+1), i+1);
		m_cbxStage->blockSignals(false);
		connect(m_cbxStage, &QComboBox::currentIndexChanged, this, &ClassesWidget::reload, Qt::UniqueConnection);
	}
	reload();
}

void ClassesWidget::reload()
{
	if(!getPlugin<EventPlugin>()->isEventOpen()) {
		m_classesModel->clearRows();
		m_courseCodesModel->clearRows();
		return;
	}
	int stage_id = selectedStageId();
	{
		qf::core::sql::QueryBuilder qb1;
		qb1.select("COUNT(runs.isRunning)")
				.from("runs")
				.joinRestricted("runs.competitorId", "competitors.id",
								"competitors.classId=classdefs.classId"
								" AND runs.isRunning"
								" AND runs.stageId=" QF_IARG(stage_id)
				, qf::core::sql::QueryBuilder::INNER_JOIN);
		qfDebug() << qb1.toString();
		qf::core::sql::QueryBuilder qb2;
		qb2.select("COUNT(relays.isRunning)")
			.from("relays")
			.where("relays.classId=classdefs.classId"
							" AND relays.isRunning");
		qfDebug() << qb2.toString();
		qfs::QueryBuilder qb;
		qb.select2("classes", "*")
				.select2("classdefs", "*")
				.select2("courses", "id, name, length, climb")
				.select("(" + qb1.toString() + ") AS runsCount")
				.select("(" + qb2.toString() + ") AS relaysCount")
				.from("classes")
				.joinRestricted("classes.id", "classdefs.classId", "classdefs.stageId=" QF_IARG(stage_id))
				.join("classdefs.courseId", "courses.id")
				.orderBy("classes.name");//.limit(10);
		qfDebug() << qb.toString();
		/*
		int class_id = m_cbxClasses->currentData().toInt();
		if(class_id > 0) {
			qb.where("competitors.classId=" + QString::number(class_id));
		}
		*/
		m_classesModel->setQueryBuilder(qb, false);
		m_classesModel->reload();
	}
	/*
	{
		qf::core::sql::Query q(m_classesModel->sqlConnection());
		q.exec("SELECT COUNT(*) FROM courses");
		bool ro = true;
		if(q.next())
			ro = (q.value(0).toInt() == 0);
		if(ro) {
			qfWarning() << tr("Courses are not imported, class table is read only.");
		}
		ui->tblClasses->setReadOnly(ro);
	}
	*/
	{
		QMap<int, QString> courses;
		qf::core::sql::Query q;
		q.exec("SELECT id, name, note FROM courses ORDER BY name, note");
		while(q.next()) {
			courses[q.value(0).toInt()] = q.value(1).toString() + ' ' + q.value(2).toString();
		}
		m_courseItemDelegate->setCourses(courses);
	}
	ui->chkUseAllMaps->setChecked(getPlugin<EventPlugin>()->stageData(stage_id).isUseAllMaps());
	reloadCourseCodes();
}

void ClassesWidget::reloadCourseCodes()
{
	int current_course_id = 0;
	auto row = ui->tblClasses->selectedRow();
	if(!row.isNull())
		current_course_id = row.value("classdefs.courseId").toInt();
	if(current_course_id == 0) {
		ui->lblCourseCodes->setText("---");
	}
	else {
		ui->lblCourseCodes->setText(row.value("courses.name").toString());
	}
	{
		qfs::QueryBuilder qb;
		qb.select2("codes", "*")
				.select2("coursecodes", "position")
				.select("'' AS control_type")
				.from("coursecodes")
				.join("coursecodes.codeId", "codes.id")
				.where("coursecodes.courseId=" QF_IARG(current_course_id))
				.orderBy("coursecodes.position");
		m_courseCodesModel->setQueryBuilder(qb, false);
		m_courseCodesModel->reload();
	}
}

void ClassesWidget::importCourses(const QList<ImportCourseDef> &course_defs, const QList<quickevent::core::CodeDef> &code_defs)
{
	qf::gui::framework::MainWindow *fwk = qf::gui::framework::MainWindow::frameWork();
	QString msg = tr("Delete all courses definitions for stage %1?").arg(selectedStageId());
	bool delete_current = qfd::MessageBox::askYesNo(fwk, msg, false);
	getPlugin<ClassesPlugin>()->createCourses(selectedStageId(), course_defs, code_defs, delete_current);
	reload();
}

static QString normalize_course_name(const QString &course_name)
{
	QString ret = qf::core::Collator::toAscii7(QLocale::Czech, course_name, false);
	ret.replace(' ', QString());
	ret.replace(';', '+');
	ret.replace(',', '+');
	ret.replace(':', '+');
	ret.replace('-', '+');
	return ret;
}

void ClassesWidget::import_ocad_txt()
{
	if (getPlugin<EventPlugin>()->eventConfig()->isRelays()) {
		QMessageBox::warning(this,tr("Warning"),tr("Import does not yet support relays."));
		return;
	}

	QString fn = qfd::FileDialog::getOpenFileName(this, tr("Open file"));
	if(fn.isEmpty())
		return;
	QFile f(fn);
	if(f.open(QFile::ReadOnly)) {
		QStringList lines;
		while (true) {
			QByteArray ba = f.readLine();
			if(ba.isEmpty())
				break;
			lines << QString::fromUtf8(ba).trimmed();
		}
		try {
			QMap<QString, ImportCourseDef> defined_courses_map;
			enum {ColCourseName = 0, ColLenght, ColClimb, ColCodesCount, ColCodes};
			for(const auto &line : lines) {
				// coursename lenght_km climb codes_count S1-code_1[-code_n]-F1
				if(line.isEmpty())
					continue;

				QStringList sections = line.split('\t', SkipEmptyParts);
				QStringList class_names;

				qfc::String course_name = normalize_course_name(sections.value(ColCourseName));
				class_names = course_name.splitAndTrim('+');
				//qfInfo() << course_name << class_names;
				if(class_names.isEmpty()) {
					//class_names << course_name;
					qfWarning() << "cannot deduce class name, skipping line:" << line;
					continue;
				}
				if(defined_courses_map.contains(course_name)) {
					ImportCourseDef cd = defined_courses_map.value(course_name);
					QStringList classes = cd.classes() << class_names;
					defined_courses_map[course_name].setClasses(classes);
					continue;
				}
				ImportCourseDef &cd = defined_courses_map[course_name];
				cd.setName(course_name);
				cd.setClasses(class_names);
				{
					QString s = sections.value(ColLenght);
					s.replace(',', '.');
					cd.setLenght((int)(s.toDouble() * 1000));
				}
				{
					QString s = sections.value(ColClimb);
					cd.setClimb(s.toInt());
				}
				{
					QString s = sections.value(ColCodes);
					QVariantList codes;
					QStringList sl = s.split('-');
					for (int i = 1; i < sl.count()-1; i++) {
						codes << quickevent::core::CodeDef::codeFromString(sl[i]);
					}
					cd.setCodes(codes);
				}
			}
			importCourses(defined_courses_map.values(), QList<quickevent::core::CodeDef>());
		}
		catch (const qf::core::Exception &e) {
			qf::gui::framework::MainWindow *fwk = qf::gui::framework::MainWindow::frameWork();
			qf::gui::dialogs::MessageBox::showException(fwk, e);
		}
	}
}

void ClassesWidget::import_ocad_v8()
{
	QString fn = qfd::FileDialog::getOpenFileName(this, tr("Open file"));
	if(fn.isEmpty())
		return;
	QFile f(fn);
	if(f.open(QFile::ReadOnly)) {
		QStringList lines;
		while (true) {
			QByteArray ba = f.readLine();
			if(ba.isEmpty())
				break;
			lines << QString::fromUtf8(ba).trimmed();
		}
		try {
			bool is_relays = getPlugin<EventPlugin>()->eventConfig()->isRelays();
			QMap<QString, ImportCourseDef> defined_courses_map;
			for(const auto &line : lines) {
				// [classname];coursename;[relay.leg];lenght_km;climb;S1;dist_1;code_1[;dist_n;code_n];dist_finish;F1
				if(line.isEmpty())
					continue;
				QStringList class_names;
				QString class_name;
				QString course_name;
				if(is_relays) {
					class_name = line.section(';', 1, 1);
					course_name = line.section(';', 2, 2);
					if(!class_names.contains(class_name))
						class_names << class_name;
				}
				else {
					class_name = line.section(';', 0, 0);
					course_name = normalize_course_name(line.section(';', 1, 1));
					if(class_name.isEmpty()) {
						for(auto ch : {'-', ',', ':', '+'}) {
							if(course_name.contains(ch)) {
								class_names = qfc::String(course_name).splitAndTrim(ch);
								break;
							}
						}
						if(class_names.isEmpty() && !course_name.isEmpty())
							class_names << course_name;
					}
					else {
						class_names << class_name;
					}
					if(class_names.isEmpty()) {
						//class_names << course_name;
						qfWarning() << "cannot deduce class name, skipping line:" << line;
						continue;
					}
				}
				if(defined_courses_map.contains(course_name)) {
					ImportCourseDef cd = defined_courses_map.value(course_name);
					QStringList classes = cd.classes() << class_names;
					defined_courses_map[course_name].setClasses(classes);
					continue;
				}
				ImportCourseDef &cd = defined_courses_map[course_name];
				cd.setName(course_name);
				cd.setClasses(class_names);
				{
					QString s = line.section(';', 3, 3).trimmed();
					s.replace('.', QString()).replace(',', QString());
					cd.setLenght(s.toInt());
				}
				{
					QString s = line.section(';', 4, 4).trimmed();
					s.replace('.', QString()).replace(',', QString());
					cd.setClimb(s.toInt());
				}
				{
					qfc::String s = line.section(';', 6);
					QVariantList codes;
					QStringList sl = s.splitAndTrim(';');
					for (int i = 0; i < sl.count()-2; i+=2) {
						codes << quickevent::core::CodeDef::codeFromString(sl[i+1]);
					}
					cd.setCodes(codes);
				}
			}
			importCourses(defined_courses_map.values(), QList<quickevent::core::CodeDef>());
		}
		catch (const qf::core::Exception &e) {
			qf::gui::framework::MainWindow *fwk = qf::gui::framework::MainWindow::frameWork();
			qf::gui::dialogs::MessageBox::showException(fwk, e);
		}
	}
}

static QString element_text(const QDomElement &parent, const QString &tag_name)
{
	QDomElement el = parent.firstChildElement(tag_name);
	if(el.isNull())
		qfWarning() << parent.tagName() << "does not have child:" << tag_name;
	return el.text();
}

static QString dump_element(const QDomElement &el)
{
	QString ret;
	QTextStream s(&ret);
	el.save(s, QDomNode::EncodingFromDocument);
	return ret;
}

void ClassesWidget::import_ocad_iofxml_2()
{
	if (getPlugin<EventPlugin>()->eventConfig()->isRelays()) {
		QMessageBox::warning(this,tr("Warning"),tr("Import does not yet support relays."));
		return;
	}

	qfLogFuncFrame();
	QString fn = qfd::FileDialog::getOpenFileName(this, tr("Open file"), QString(), tr("XML files (*.xml);; All files (*)"));
	if(fn.isEmpty())
		return;
	try {
		QFile f(fn);
		if(f.open(QFile::ReadOnly)) {
			QDomDocument xdoc;
#if QT_VERSION < QT_VERSION_CHECK(6, 8, 0)
			QString err_str; int err_line;
			if(!xdoc.setContent(&f, &err_str, &err_line)) {
				QF_EXCEPTION(QString("Error parsing xml file '%1' at line: %2").arg(err_str).arg(err_line));
			}
#else
			if(auto res = xdoc.setContent(&f); !res) {
				QF_EXCEPTION(QString("Error parsing xml file '%1' at line: %2").arg(res.errorMessage).arg(res.errorLine));
			}
#endif
			QDomNodeList xml_courses = xdoc.elementsByTagName(QStringLiteral("Course"));

			QList<ImportCourseDef> defined_courses_list;
			for (int i = 0; i < xml_courses.count(); ++i) {
				QDomElement el_course = xml_courses.at(i).toElement();
				if(el_course.isNull())
					QF_EXCEPTION(QString("Xml file format error: bad element '%1'").arg("Course"));
				ImportCourseDef coursedef;
				QString course_name = normalize_course_name(element_text(el_course, QStringLiteral("CourseName")));
				coursedef.setName(course_name);

				QStringList class_names;
				QDomNodeList xml_classes = el_course.elementsByTagName(QStringLiteral("ClassShortName"));
				for (int j = 0; j < xml_classes.count(); ++j) {
					QString class_name = xml_classes.at(j).toElement().text().trimmed();
					class_names << class_name;
				}
				if(class_names.isEmpty())
					class_names << course_name;
				coursedef.setClasses(class_names);

				QDomElement el_course_variantion = el_course.firstChildElement(QStringLiteral("CourseVariation"));
				if(el_course_variantion.isNull())
					QF_EXCEPTION(QString("Xml file format error: missing element '%1'").arg("CourseVariation"));
				coursedef.setLenght(element_text(el_course_variantion, QStringLiteral("CourseLength")).trimmed().toInt());
				coursedef.setClimb(element_text(el_course_variantion, QStringLiteral("CourseClimb")).trimmed().toInt());

				QMap<int, QVariant> codes;
				QDomNodeList xml_controls = el_course.elementsByTagName(QStringLiteral("CourseControl"));
				for (int j = 0; j < xml_controls.count(); ++j) {
					QDomElement el_control = xml_controls.at(j).toElement();
					int no = element_text(el_control, QStringLiteral("Sequence")).trimmed().toInt();
					if(no <= 0)
						QF_EXCEPTION(QString("Xml file format error: bad sequence number %1 in %2").arg(no).arg(dump_element(el_control)));
					auto code_str = element_text(el_control, QStringLiteral("ControlCode")).trimmed();
					codes[no] = quickevent::core::CodeDef::codeFromString(code_str);
				}
				coursedef.setCodes(codes.values());
				defined_courses_list << coursedef;
			}
			importCourses(defined_courses_list, QList<quickevent::core::CodeDef>());
		}
	}
	catch (const qf::core::Exception &e) {
		qf::gui::framework::MainWindow *fwk = qf::gui::framework::MainWindow::frameWork();
		qf::gui::dialogs::MessageBox::showException(fwk, e);
	}
}

void ClassesWidget::import_ocad_iofxml_3()
{
	if (getPlugin<EventPlugin>()->eventConfig()->isRelays()) {
		QMessageBox::warning(this,tr("Warning"),tr("Import does not yet support relays."));
		return;
	}

	qfLogFuncFrame();
	//static constexpr int START_CODE0 = 0;
	//static const int FINISH_CODE0 = quickevent::core::CodeDef::FINISH_PUNCH_CODE - 1;
	QString fn = qfd::FileDialog::getOpenFileName(this, tr("Open file"), QString(), "XML files (*.xml);; All files (*)");
	if(fn.isEmpty())
		return;
	try {
		QFile f(fn);
		if(f.open(QFile::ReadOnly)) {
			QDomDocument xdoc;
#if QT_VERSION < QT_VERSION_CHECK(6, 8, 0)
			QString err_str; int err_line;
			if(!xdoc.setContent(&f, &err_str, &err_line))
				QF_EXCEPTION(QString("Error parsing xml file '%1' at line: %2").arg(err_str).arg(err_line));

#else
			if(auto res = xdoc.setContent(&f); !res) {
				QF_EXCEPTION(QString("Error parsing xml file '%1' at line: %2").arg(res.errorMessage).arg(res.errorLine));
			}
#endif
			QList<quickevent::core::CodeDef> defined_codes;
			{
				QDomElement el_course_data = xdoc.elementsByTagName(QStringLiteral("RaceCourseData")).at(0).toElement();
				const auto CONTROL = QStringLiteral("Control");
				for (QDomElement el_code = el_course_data.firstChildElement(CONTROL); !el_code.isNull(); el_code = el_code.nextSiblingElement(CONTROL)) {
					quickevent::core::CodeDef codedef;
					QString code_str = element_text(el_code, QStringLiteral("Id")).trimmed();
					codedef.setCode(code_str);
					QDomElement el_pos = el_code.firstChildElement(QStringLiteral("Position"));
					codedef.setLongitude(el_pos.attribute(QStringLiteral("lng")).trimmed().toDouble());
					codedef.setLatitude(el_pos.attribute(QStringLiteral("lat")).trimmed().toDouble());
					qfDebug() << "adding code:" << codedef.toString();
					defined_codes << codedef;
				}
			}
			QMap<QString, ImportCourseDef> defined_courses;
			{
				QDomNodeList ndlst = xdoc.elementsByTagName(QStringLiteral("Course"));
				for (int i = 0; i < ndlst.count(); ++i) {
					QDomElement el_course = ndlst.at(i).toElement();
					if(el_course.isNull())
						QF_EXCEPTION(QString("Xml file format error: bad element '%1'").arg("Course"));
					QString course_name = normalize_course_name(element_text(el_course, QStringLiteral("Name")));
					if(course_name.isEmpty())
						QF_EXCEPTION(QString("Xml file format error: empty course name in '%1'").arg(dump_element(el_course)));
					ImportCourseDef &coursedef = defined_courses[course_name];
					coursedef.setName(course_name);
					coursedef.setLenght(element_text(el_course, QStringLiteral("Length")).trimmed().toInt());
					coursedef.setClimb(element_text(el_course, QStringLiteral("Climb")).trimmed().toInt());

					QVariantList codes;
					QDomNodeList xml_controls = el_course.elementsByTagName(QStringLiteral("CourseControl"));
					for (int j = 0; j < xml_controls.count(); ++j) {
						QDomElement el_control = xml_controls.at(j).toElement();
						//if(el_control.attribute(QStringLiteral("type")) != QLatin1String("Control"))
						//		continue;
						QString code_str = element_text(el_control, QStringLiteral("Control")).trimmed();
						qfDebug() << code_str;
						codes << quickevent::core::CodeDef::codeFromString(code_str);
					}
					coursedef.setCodes(codes);
				}
			}
			{
				QDomNodeList ndlst = xdoc.elementsByTagName(QStringLiteral("ClassCourseAssignment"));
				for (int i = 0; i < ndlst.count(); ++i) {
					QDomElement class_assignment = ndlst.at(i).toElement();
					QString course_name = normalize_course_name(element_text(class_assignment, QStringLiteral("CourseName")));
					QString class_name = element_text(class_assignment, QStringLiteral("ClassName"));
					if(class_name.isEmpty())
						QF_EXCEPTION(QString("Xml file format error: empty class name in '%1'").arg(dump_element(class_assignment)));
					ImportCourseDef &coursedef = defined_courses[course_name];
					coursedef.addClass(class_name);
				}
			}
			{
				// guess empty class names from course name
				QMutableMapIterator<QString, ImportCourseDef> it(defined_courses);
				while(it.hasNext()) {
					it.next();
					ImportCourseDef &cd = it.value();
					if(cd.classes().isEmpty())
						cd.setClasses(QStringList() << cd.name());
				}
			}
			{
				// split combined class names
				bool split_class_names_enabled = false;
				bool split_class_names_prompted = false;
				QMutableMapIterator<QString, ImportCourseDef> it(defined_courses);
				while(it.hasNext()) {
					it.next();
					ImportCourseDef &cd = it.value();
					QStringList class_names = cd.classes();
					QStringList split_class_names;
					for(const QString &class_name : class_names) {
						QString normalized_class_name = normalize_course_name(class_name);
						//qfInfo() << cd.name() << ":" << class_name << normalized_class_name;
						if(class_name != normalized_class_name || class_name.contains('+')) {
							if(!split_class_names_prompted) {
								split_class_names_enabled = qf::gui::dialogs::MessageBox::askYesNo(this, tr("Class name '%1' seems to be combined, separate it to more classes?").arg(class_name));
								split_class_names_prompted = true;
							}
						}
						if(split_class_names_enabled)
							split_class_names << normalized_class_name.split('+');
						else {
							split_class_names << class_name;
						}
					}
					if(split_class_names_enabled)
						cd.setClasses(split_class_names);
				}
			}
			importCourses(defined_courses.values(), defined_codes);
		}
	}
	catch (const qf::core::Exception &e) {
		qf::gui::framework::MainWindow *fwk = qf::gui::framework::MainWindow::frameWork();
		qf::gui::dialogs::MessageBox::showException(fwk, e);
	}
}

#include "classeswidget.moc"
