#include "editcoursecodeswidget.h"
#include "editcourseswidget.h"
#include "ui_editcourseswidget.h"

#include "../../Event/src/eventplugin.h"

#include <quickevent/core/codedef.h>

#include <qf/gui/framework/mainwindow.h>
#include <qf/gui/model/sqltablemodel.h>
#include <qf/core/sql/connection.h>
#include <qf/gui/dialogs/dialog.h>
#include <qf/gui/dialogbuttonbox.h>

#include <QPushButton>

using qf::gui::framework::getPlugin;
namespace qfw = qf::gui;
namespace qfm = qf::gui::model;
namespace qfs = qf::core::sql;

class CoursesTableModel : public qfm::SqlTableModel
{
	Q_OBJECT
private:
	using Super = qfm::SqlTableModel;
public:
	enum Columns {
		Col_id,
		Col_name,
		Col_length,
		Col_climb,
		Col_mapCount,
		Col_runCount,
		Col_note,
		Col_codeCount,
		Col_codeList,
		Col_COUNT
	};

	CoursesTableModel(QObject *parent) : Super(parent)
	{
		clearColumns(Col_COUNT);
		setColumn(Col_id, qf::gui::model::TableModel::ColumnDefinition("id", tr("Id")).setReadOnly(true));
		setColumn(Col_name, qf::gui::model::TableModel::ColumnDefinition("courses.name", tr("Name")));
		setColumn(Col_length, qf::gui::model::TableModel::ColumnDefinition("courses.length", tr("Length")));
		setColumn(Col_climb, qf::gui::model::TableModel::ColumnDefinition("courses.climb", tr("Climb")));
		setColumn(Col_mapCount, qf::gui::model::TableModel::ColumnDefinition("courses.mapCount", tr("Maps")));
		setColumn(Col_runCount, qf::gui::model::TableModel::ColumnDefinition("run_count", tr("Runners")));
		setColumn(Col_note, qf::gui::model::TableModel::ColumnDefinition("courses.note", tr("Note")));
		setColumn(Col_codeCount, qf::gui::model::TableModel::ColumnDefinition("code_count", tr("Code count")).setReadOnly(true));
		setColumn(Col_codeList, qf::gui::model::TableModel::ColumnDefinition("code_list", tr("Codes")).setReadOnly(true));
	}
public:
	QVariant data(const QModelIndex &index, int role) const override
	{
		if(index.column() == Col_runCount) {
			if(role == Qt::BackgroundRole) {
				auto run_count = index.data().toInt();
				QModelIndex ix = index.sibling(index.row(), Col_mapCount);
				auto map_count = ix.data().toInt();
				if(run_count > map_count)
					return QColor("salmon");
			}
		}
		return Super::data(index, role);
	}
};

EditCoursesWidget::EditCoursesWidget(int stage_id, QWidget *parent)
	: Super(parent)
	, ui(new Ui::EditCoursesWidget)
{
	setPersistentSettingsId("EditCoursesWidget");
	ui->setupUi(this);
	{
		ui->tblCourses->setPersistentSettingsId("tblCourses");
		ui->tblCourses->setDirtyRowsMenuSectionEnabled(false);
		ui->tblCoursesTB->setTableView(ui->tblCourses);
		ui->tblCourses->setRowEditorMode(qfw::TableView::RowEditorMode::EditRowsInline);
		connect(ui->tblCourses, &qfw::TableView::editCellRequest, this, [this](const QModelIndex &ix) {
			if(ix.column() == this->m_coursesModel->columnIndex("code_list")) {
				editCourseCodes(ix);
			}
		});
		auto *m = new CoursesTableModel(this);
		ui->tblCourses->setTableModel(m);
		m_coursesModel = m;
	}
	{
		qf::core::sql::Connection conn = m_coursesModel->sqlConnection();
		qfs::QueryBuilder qb_code_count;
		qb_code_count.select("COUNT(*)").from("coursecodes")
				.join("coursecodes.codeId", "codes.id")
				.where("coursecodes.courseId=courses.id AND "
					"codes.code >= " QF_IARG(quickevent::core::CodeDef::PUNCH_CODE_MIN) " AND "
					"codes.code <= " QF_IARG(quickevent::core::CodeDef::PUNCH_CODE_MAX))
				.as("code_count");

		QString control_code_query = "SELECT CAST(code AS TEXT) AS code, position"
						" FROM coursecodes INNER JOIN codes ON codes.id = coursecodes.codeId WHERE (coursecodes.courseId = courses.id)"
						" AND code >= " QF_IARG(quickevent::core::CodeDef::PUNCH_CODE_MIN)
						" AND code <= " QF_IARG(quickevent::core::CodeDef::PUNCH_CODE_MAX);
		QString start_code_query = "SELECT 'S' || (code - " QF_IARG(quickevent::core::CodeDef::START_PUNCH_CODE) ") AS code, position"
						" FROM coursecodes INNER JOIN codes ON codes.id = coursecodes.codeId WHERE (coursecodes.courseId = courses.id)"
						" AND code >= " QF_IARG(quickevent::core::CodeDef::START_PUNCH_CODE)
						" AND code < " QF_IARG(quickevent::core::CodeDef::PUNCH_CODE_MIN);
		QString finnish_code_query = "SELECT 'F' || (code - " QF_IARG(quickevent::core::CodeDef::FINISH_PUNCH_CODE) ") AS code, position "
						" FROM coursecodes INNER JOIN codes ON codes.id = coursecodes.codeId WHERE (coursecodes.courseId = courses.id)"
						" AND code >= " QF_IARG(quickevent::core::CodeDef::FINISH_PUNCH_CODE);

		QString code_list_query = start_code_query + " UNION " + control_code_query + " UNION " + finnish_code_query + " ORDER BY position";
		if(conn.driverName().endsWith(QLatin1String("PSQL"), Qt::CaseInsensitive)) {
			code_list_query = "(SELECT string_agg(code, ',') FROM (" + code_list_query + ") AS code_list_query )";
		}
		else {
			code_list_query = "(SELECT GROUP_CONCAT(code) FROM (" + code_list_query + ") )";
		}

		qfs::QueryBuilder qb;
		if (getPlugin<Event::EventPlugin>()->eventConfig()->isRelays()) {
			qb.select2("courses", "*")
					.select("0 AS run_count")
					.select(code_list_query + "AS code_list")
					.select(qb_code_count.toString())
					.from("courses")
					.orderBy("courses.name");
		} else {
			qb.select2("courses", "*")
					.select("COUNT(runs.id) AS run_count")
					.select(code_list_query + "AS code_list")
					.select(qb_code_count.toString())
					.from("courses")
					.join("courses.id", "classdefs.courseId", qf::core::sql::QueryBuilder::INNER_JOIN)
					.join("classdefs.classId", "classes.id")
					.join("classes.id", "competitors.classId")
					.joinRestricted("competitors.id", "runs.competitorId", "runs.isRunning")
					.where("classdefs.stageId=" QF_IARG(stage_id))
					.groupBy("courses.id")
					.orderBy("courses.name");
		}

		m_coursesModel->setQueryBuilder(qb, false);
		m_coursesModel->reload();
	}
}

EditCoursesWidget::~EditCoursesWidget()
{
	delete ui;
}

void EditCoursesWidget::editCourseCodes(const QModelIndex &ix)
{
	auto *m = ui->tblCourses->tableModel();
	if(!m)
		return;
	int row_no = ui->tblCourses->toTableModelRowNo(ix.row());
	if(row_no < 0)
		return;
	int course_id = m->tableRow(row_no).value("courses.id").toInt();
	if(course_id <= 0)
		return;
	qf::gui::dialogs::Dialog dlg(QDialogButtonBox::Close | QDialogButtonBox::Ok | QDialogButtonBox::Apply | QDialogButtonBox::Reset, this);
	auto *w = new EditCourseCodesWidget();
	connect(w, &EditCourseCodesWidget::courseCodesSaved, ui->tblCourses, &qfw::TableView::reloadCurrentRow);
	{
		QPushButton *bt = dlg.buttonBox()->button(QDialogButtonBox::Apply);
		connect(bt, &QPushButton::clicked, w, &EditCourseCodesWidget::save);
	}
	{
		QPushButton *bt = dlg.buttonBox()->button(QDialogButtonBox::Reset);
		connect(bt, &QPushButton::clicked, w, &EditCourseCodesWidget::reload);
	}
	w->reload(course_id);
	dlg.setCentralWidget(w);
	dlg.exec();
}

#include "editcourseswidget.moc"
