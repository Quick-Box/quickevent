#include "runswidget.h"
#include "ui_runswidget.h"
#include "runstablemodel.h"
#include "runsplugin.h"

#include <plugins/Competitors/src/competitorwidget.h>
#include <plugins/Competitors/src/competitordocument.h>
#include <plugins/Event/src/eventplugin.h>

#include <quickevent/gui/og/sqltablemodel.h>
#include <quickevent/gui/og/itemdelegate.h>
#include <quickevent/core/exporters/stageresultscsvexporter.h>

#include <qf/gui/dialogs/dialog.h>
#include <qf/gui/framework/mainwindow.h>
#include <qf/gui/dialogs/messagebox.h>
#include <qf/gui/dialogs/filedialog.h>
#include <qf/gui/reports/widgets/reportviewwidget.h>
#include <qf/gui/menubar.h>
#include <qf/gui/action.h>
#include <qf/gui/toolbar.h>
#include <qf/gui/combobox.h>
#include <qf/gui/tableview.h>
#include <qf/gui/dialogbuttonbox.h>

#include <qf/gui/model/sqltablemodel.h>
#include <qf/core/sql/querybuilder.h>
#include <qf/core/sql/dbenum.h>
#include <qf/core/sql/connection.h>
#include <qf/core/sql/query.h>
#include <qf/core/sql/transaction.h>
#include <qf/core/assert.h>

#include <QCheckBox>
#include <QDateTime>
#include <QLabel>
#include <QInputDialog>
#include <QTimer>
#include <QRandomGenerator>

#include <algorithm>

#if QT_VERSION < QT_VERSION_CHECK(5, 14, 0)
static const auto SkipEmptyParts = QString::SkipEmptyParts;
#else
static const auto SkipEmptyParts = Qt::SkipEmptyParts;
#endif

namespace qfc = qf::core;
namespace qfu = qf::core::utils;
namespace qfs = qf::core::sql;
namespace qfw = qf::gui;
namespace qff = qf::gui::framework;
namespace qfd = qf::gui::dialogs;
namespace qfm = qf::gui::model;

using qf::gui::framework::getPlugin;
using Event::EventPlugin;
using Runs::RunsPlugin;

RunsWidget::RunsWidget(QWidget *parent) :
	Super(parent),
	ui(new Ui::RunsWidget)
{
	ui->setupUi(this);

	//bool is_relays = getPlugin<EventPlugin>()->eventConfig()->isRelays();

	ui->frmDrawing->setVisible(false);

	connect(getPlugin<EventPlugin>(), &Event::EventPlugin::eventOpenChanged, this, [this]() {
		ui->cbxDrawMethod->clear();
		if(!getPlugin<EventPlugin>()->isEventOpen())
			return;
		bool is_relays = getPlugin<EventPlugin>()->eventConfig()->isRelays();
		if(is_relays) {
			ui->cbxDrawMethod->addItem(tr("Relays first leg"), static_cast<int>(DrawMethod::RelaysFirstLeg));
		}
		else {
			ui->cbxDrawMethod->addItem(tr("Randomized equidistant clubs"), static_cast<int>(DrawMethod::RandomizedEquidistantClubs));
			ui->cbxDrawMethod->addItem(tr("Random number"), static_cast<int>(DrawMethod::RandomNumber));
			ui->cbxDrawMethod->addItem(tr("Equidistant clubs"), static_cast<int>(DrawMethod::EquidistantClubs));
			ui->cbxDrawMethod->addItem(tr("Stage 1 reverse order"), static_cast<int>(DrawMethod::StageReverseOrder));
			ui->cbxDrawMethod->addItem(tr("Handicap"), static_cast<int>(DrawMethod::Handicap));
			ui->cbxDrawMethod->addItem(tr("Keep runners order"), static_cast<int>(DrawMethod::KeepOrder));
			ui->cbxDrawMethod->addItem(tr("Grouped: C, B+A (PSOB DH12-14)"), static_cast<int>(DrawMethod::GroupedC));
			ui->cbxDrawMethod->addItem(tr("Grouped: C, B, A+E+R (PSOB DH16-20)"), static_cast<int>(DrawMethod::GroupedCB));
			ui->cbxDrawMethod->addItem(tr("Grouped by ranking (PSOB DH21L)"), static_cast<int>(DrawMethod::GroupedRanking));
		}
	});
	// connect(ui->wRunsTableWidget->tableView(), &qfw::TableView::editRowInExternalEditor, this, &RunsWidget::editCompetitor, Qt::QueuedConnection);
	connect(ui->wRunsTableWidget, &RunsTableWidget::editCompetitorRequest, this, &RunsWidget::editCompetitor, Qt::QueuedConnection);
	connect(ui->wRunsTableWidget->tableView(), &qfw::TableView::editSelectedRowsInExternalEditor, this, &RunsWidget::editCompetitors, Qt::QueuedConnection);
	connect(ui->btDraw, &QAbstractButton::clicked, this, &RunsWidget::onDrawClicked);
	connect(ui->btDrawRemove, &QAbstractButton::clicked, this, &RunsWidget::onDrawRemoveClicked);
}

RunsWidget::~RunsWidget()
{
	delete ui;
}

void RunsWidget::reset(int class_id)
{
	qfLogFuncFrame();
	if(!getPlugin<EventPlugin>()->isEventOpen()) {
		ui->wRunsTableWidget->clear();
		return;
	}
	{
		m_cbxStage->blockSignals(true);
		m_cbxStage->clear();
		for(int i=0; i<getPlugin<EventPlugin>()->stageCount(); i++)
			m_cbxStage->addItem(tr("E%1").arg(i+1), i+1);
		connect(m_cbxStage, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &RunsWidget::reload, Qt::UniqueConnection);
		connect(m_cbxStage, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &RunsWidget::onCbxStageCurrentIndexChanged, Qt::UniqueConnection);
		m_cbxStage->blockSignals(false);
	}
	/// Note: You should use QAction::setVisible() to change the visibility of the widget.
	/// Using QWidget::setVisible(), QWidget::show() and QWidget::hide() does not work.
	bool is_relays = getPlugin<EventPlugin>()->eventConfig()->isRelays();
	m_toolbarActionLabelLeg->setVisible(is_relays);
	m_toolbarActionComboLeg->setVisible(is_relays);
	//qfWarning() << "is relays:" << is_relays << "legs visible:" << m_cbxLeg->isVisible();
	{
		m_cbxClasses->blockSignals(true);
		m_cbxClasses->loadItems(true);
		m_cbxClasses->insertItem(0, tr("--- all ---"), 0);
		if(class_id <= 0)
			m_cbxClasses->setCurrentIndex(0);
		else
			m_cbxClasses->setCurrentData(class_id);
		connect(m_cbxClasses, &qf::gui::ForeignKeyComboBox::currentDataChanged, this, &RunsWidget::reload, Qt::UniqueConnection);
		m_cbxClasses->blockSignals(false);
	}
	reload();
}

void RunsWidget::reload()
{
	qfLogFuncFrame();
	bool is_relays = getPlugin<EventPlugin>()->eventConfig()->isRelays();
	int stage_id = is_relays? m_cbxLeg->currentData().toInt(): selectedStageId();
	int class_id = m_cbxClasses->currentData().toInt();
	ui->wRunsTableWidget->reload(stage_id, class_id, m_chkShowOffRace->isChecked());
}

void RunsWidget::settleDownInPartWidget(::PartWidget *part_widget)
{
	qfLogFuncFrame();
	connect(part_widget, &::PartWidget::resetPartRequest, this, [this]() { reset(); });
	connect(part_widget, &::PartWidget::reloadPartRequest, this, &RunsWidget::reload);

	connect(getPlugin<RunsPlugin>(), &Runs::RunsPlugin::editCompetitorOnPunchRequest, this, &RunsWidget::editCompetitorOnPunch);

	auto *a_print = part_widget->menuBar()->actionForPath("print", true);
	a_print->setText(tr("&Print"));
	{
		auto *m = a_print->addMenuInto("startList", tr("&Start list"));
		{
			{
				auto *a = new qfw::Action(tr("&Classes"));
				connect(a, &qfw::Action::triggered, getPlugin<RunsPlugin>(), &Runs::RunsPlugin::report_startListClasses);
				m->addActionInto(a);
			}
			{
				auto *a = new qfw::Action(tr("C&lubs"));
				connect(a, &qfw::Action::triggered, getPlugin<RunsPlugin>(), &Runs::RunsPlugin::report_startListClubs);
				m->addActionInto(a);
			}
			{
				auto *a = new qfw::Action(tr("&Starters"));
				connect(a, &qfw::Action::triggered, getPlugin<RunsPlugin>(), &Runs::RunsPlugin::report_startListStarters);
				m->addActionInto(a);
			}
			m->addSeparatorInto();
			{
				auto *a = new qfw::Action(tr("Classes n stages"));
				connect(a, &qfw::Action::triggered, getPlugin<RunsPlugin>(), &Runs::RunsPlugin::report_startListClassesNStages);
				m->addActionInto(a);
			}
			{
				auto *a = new qfw::Action(tr("Clubs n stages"));
				connect(a, &qfw::Action::triggered, getPlugin<RunsPlugin>(), &Runs::RunsPlugin::report_startListClubsNStages);
				m->addActionInto(a);
			}
		}
	}
	{
		auto *m = a_print->addMenuInto("results", tr("&Results"));
		{
			{
				auto *a = new qfw::Action(tr("&Current stage"));
				a->setShortcut("Ctrl+P");
				connect(a, &qfw::Action::triggered, getPlugin<RunsPlugin>(), &Runs::RunsPlugin::report_resultsClasses);
				m->addActionInto(a);
			}
			{
				auto *a = new qfw::Action(tr("Current stage for speaker"));
				connect(a, &qfw::Action::triggered, getPlugin<RunsPlugin>(), &Runs::RunsPlugin::report_resultsForSpeaker);
				m->addActionInto(a);
			}
			m->addSeparatorInto();
			{
				auto *a = new qfw::Action(tr("Current stage awards"));
				connect(a, &qfw::Action::triggered, getPlugin<RunsPlugin>(), &Runs::RunsPlugin::report_resultsAwards);
				m->addActionInto(a);
			}
			m->addSeparatorInto();
			{
				auto *a = new qfw::Action(tr("&After n stages"));
				connect(a, &qfw::Action::triggered, getPlugin<RunsPlugin>(), &Runs::RunsPlugin::report_resultsNStages);
				m->addActionInto(a);
			}
			{
				auto *a = new qfw::Action(tr("&After n stages for speaker"));
				connect(a, &qfw::Action::triggered, getPlugin<RunsPlugin>(), &Runs::RunsPlugin::report_resultsNStagesSpeaker);
				m->addActionInto(a);
			}
			{
				auto *a = new qfw::Action(tr("N stages awards"));
				connect(a, &qfw::Action::triggered, getPlugin<RunsPlugin>(), &Runs::RunsPlugin::report_nStagesAwards);
				m->addActionInto(a);
			}
		}
	}
	a_print->addSeparatorInto();
	{
		auto *a = new qfw::Action(tr("Competitors statistics"));
		//a->setShortcut("ctrl+L");
		connect(a, &qfw::Action::triggered, this, &RunsWidget::report_competitorsStatistics);
		a_print->addActionInto(a);
	}
	{
		auto *a = new qfw::Action(tr("&Competitors with rented cards"));
		connect(a, &qfw::Action::triggered, this, [this]() {
			qff::MainWindow *fwk = qff::MainWindow::frameWork();
			quickevent::gui::ReportOptionsDialog dlg(fwk);
			dlg.setPersistentSettingsId("competitorsWithRentedCards");
			dlg.loadPersistentSettings();
			dlg.setClassFilterVisible(false);
			dlg.setStartListOptionsVisible(false);
			dlg.setStartListPrintVacantsVisible(false);
			dlg.setPageLayoutVisible(true);
			dlg.setStartTimeFormatVisible(false);
			dlg.setStartlistOrderFirstByVisible(false);
			if(!dlg.exec())
				return;
			auto opts = dlg.optionsMap();
			QVariantMap props;
			props["stageId"] = selectedStageId();
			props["options"] = opts;
			qf::gui::reports::ReportViewWidget::showReport(fwk
										, getPlugin<RunsPlugin>()->findReportFile("competitorsWithCardRent.qml")
										, QVariant()
										, tr("Competitors with rented cards")
										, "printReport"
										, props
										);
		});
		a_print->addActionInto(a);
	}

	qfw::Action *a_import = part_widget->menuBar()->actionForPath("import", true);
	a_import->setText(tr("&Import"));

	qfw::Action *a_import_start_times = a_import->addMenuInto("startTimes", tr("Start times"));
	auto *a_import_start_times_ob2000 = new qfw::Action("ob2000", tr("OB 2000"));
	a_import_start_times->addActionInto(a_import_start_times_ob2000);
	connect(a_import_start_times_ob2000, &qfw::Action::triggered, this, &RunsWidget::import_start_times_ob2000);

	auto *a_export = part_widget->menuBar()->actionForPath("export", true);
	a_export->setText(tr("E&xport"));
	auto *m_stlist = a_export->addMenuInto("startList", tr("&Start list"));
	auto *m_export_stlist_html = m_stlist->addMenuInto("html", tr("&HTML"));
	{
		{
			auto *a = new qfw::Action(tr("&Classes"));
			connect(a, &qfw::Action::triggered, getPlugin<RunsPlugin>(), &Runs::RunsPlugin::export_startListClassesHtml);
			m_export_stlist_html->addActionInto(a);
		}
		{
			auto *a = new qfw::Action(tr("C&lubs"));
			connect(a, &qfw::Action::triggered, getPlugin<RunsPlugin>(), &Runs::RunsPlugin::export_startListClubsHtml);
			m_export_stlist_html->addActionInto(a);
		}
	}
	auto *m_export_stlist_xml = m_stlist->addMenuInto("xml", tr("&XML"));
	{
		{
			auto *a = new qfw::Action(tr("&IOF-XML 3.0"));
			connect(a, &qfw::Action::triggered, this, &RunsWidget::export_startList_stage_iofxml30);
			m_export_stlist_xml->addActionInto(a);
		}
	}
	auto *m_export_stlist_csv = m_stlist->addMenuInto("csv", tr("&CSV"));
	{
		{
			auto *a = new qfw::Action(tr("&SIME startlist (Starter Clock)"));
			connect(a, &qfw::Action::triggered, this, &RunsWidget::export_startList_stage_csv_sime);
			m_export_stlist_csv->addActionInto(a);
		}
		{
			auto *a = new qfw::Action(tr("&Startlist for TV Graphics"));
			connect(a, &qfw::Action::triggered, this, &RunsWidget::export_startList_stage_tv_graphics);
			m_export_stlist_csv->addActionInto(a);
		}
	}

	qfw::Action *a_export_results = a_export->addMenuInto("results", tr("Results"));
	qfw::Action *a_export_results_stage = a_export_results->addMenuInto("currentStage", tr("Current stage"));
	{
		//a_export_results->addActionInto("iofxml23", tr("IOF XML &2.3"));
		{
			qfw::Action *a = a_export_results_stage->addActionInto("iofxml30", tr("IOF XML &3.0"));
			connect(a, &qfw::Action::triggered, this, &RunsWidget::export_results_stage_iofxml30);
		}
		{
			qfw::Action *a = a_export_results_stage->addActionInto("html", tr("&HTML"));
			connect(a, &qfw::Action::triggered, getPlugin<RunsPlugin>(), &Runs::RunsPlugin::export_resultsHtmlStage);
		}
		{
			qfw::Action *a = a_export_results_stage->addActionInto("htmlWithLaps", tr("HTML with &laps"));
			connect(a, &qfw::Action::triggered, getPlugin<RunsPlugin>(), &Runs::RunsPlugin::export_resultsHtmlStageWithLaps);
		}
		{
			qfw::Action *a = a_export_results_stage->addActionInto("csos", tr("CSOS"));
			connect(a, &qfw::Action::triggered, this, &RunsWidget::export_results_stage_csos);
		}
		{
			qfw::Action *a = a_export_results_stage->addActionInto("csv", tr("&CSV"));
			connect(a, &qfw::Action::triggered, this, &RunsWidget::export_results_stage_csv);
		}
	}
	qfw::Action *a_export_results_overall = a_export_results->addMenuInto("overall", tr("Overall"));
	{
		{
			qfw::Action *a = a_export_results_overall->addActionInto("html", tr("&HTML"));
			connect(a, &qfw::Action::triggered, getPlugin<RunsPlugin>(), &Runs::RunsPlugin::export_resultsHtmlNStages);
		}
		{
			qfw::Action *a = a_export_results_overall->addActionInto("csos", tr("CSOS"));
			connect(a, &qfw::Action::triggered, this, &RunsWidget::export_results_overall_csos);
		}
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
	main_tb->addSeparator();
	QLabel *lbl_classes;
	{
		lbl_classes = new QLabel(tr(" &Class "));
		main_tb->addWidget(lbl_classes);
	}
	{
		m_cbxClasses = new qfw::ForeignKeyComboBox();
		m_cbxClasses->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);
		m_cbxClasses->setMinimumWidth(fontMetrics().horizontalAdvance('X') * 15);
		m_cbxClasses->setMaxVisibleItems(100);
		m_cbxClasses->setReferencedTable("classes");
		m_cbxClasses->setReferencedField("id");
		m_cbxClasses->setReferencedCaptionField("name");
		main_tb->addWidget(m_cbxClasses);
	}
	lbl_classes->setBuddy(m_cbxClasses);
	{
		auto *lbl_leg = new QLabel(tr("&Leg "));
		m_toolbarActionLabelLeg = main_tb->addWidget(lbl_leg);
		m_cbxLeg = new QComboBox();
		{
			m_cbxLeg->addItem(tr("--- all ---"), 0);
			m_cbxLeg->addItem("1", 1);
			m_cbxLeg->addItem("2", 2);
			m_cbxLeg->addItem("3", 3);
			m_cbxLeg->addItem("4", 4);
			m_cbxLeg->addItem("5", 5);
			m_cbxLeg->addItem("6", 6);
			m_cbxLeg->addItem("7", 7);
			m_cbxLeg->addItem("8", 8);
			m_cbxLeg->addItem("9", 9);
			m_cbxLeg->addItem("10", 10);
			connect(m_cbxLeg, SIGNAL(currentIndexChanged(int)), this, SLOT(reload()));
			m_toolbarActionComboLeg = main_tb->addWidget(m_cbxLeg);
		}
		lbl_leg->setBuddy(m_cbxLeg);
	}
	main_tb->addSeparator();

	{
		m_chkShowOffRace = new QCheckBox();
		m_chkShowOffRace->setText(tr("Show o&ff-race"));
		m_chkShowOffRace->setToolTip(tr("Include competitors who are not running in this stage"));
		connect(m_chkShowOffRace, &QCheckBox::toggled, this, &RunsWidget::reload);
		main_tb->addWidget(m_chkShowOffRace);
	}
	{
		auto *bt = new QPushButton(tr("&Draw options"));
		main_tb->addWidget(bt);
		bt->setCheckable(true);
		connect(bt, &QPushButton::toggled, ui->frmDrawing, &QFrame::setVisible);
	}
}

static bool list_length_greater_than(const QList<int> &lst1, const QList<int> &lst2)
{
	return lst1.count() > lst2.count();
}

static void shuffle(QList<int> &lst)
{
	std::random_device rd;
	std::mt19937 g(rd());
	std::shuffle(lst.begin(), lst.end(), g);
}

QList< QList<int> > RunsWidget::runnersByClubSortedByCount(int stage_id, int class_id, QMap<int, QString> &runner_id_to_club)
{
	qfLogFuncFrame();
	QMap<QString, QList<int> > ids_by_clubs;
	qf::core::sql::QueryBuilder qb;
	qb.select2("runs", "id")
			.select2("competitors", "registration")
			.from("competitors")
			.joinRestricted("competitors.id", "runs.competitorId", "runs.isRunning AND runs.stageId=" QF_IARG(stage_id), qf::core::sql::QueryBuilder::INNER_JOIN)
			.where("competitors.classId=" QF_IARG(class_id));
	qfDebug() << qb.toString();
	qfs::Query q;
	q.exec(qb.toString(), qf::core::Exception::Throw);
	while(q.next()) {
		QString club = q.value("registration").toString().mid(0, 3).trimmed().toUpper();
		int id = q.value("runs.id").toInt();
		ids_by_clubs[club] << id;
		runner_id_to_club[id] = club;
	}
	{
		for(const auto &[club, val] : ids_by_clubs.asKeyValueRange()) {
			QStringList sl;
			for(auto id : val) {
				sl << QString::number(id);
			}
			qfDebug() << "\t" << club << ":" << sl.join(", ");
		}
	}
	QList< QList<int> > ret = ids_by_clubs.values();
	// use list_length_greater_than to sort DESC
	std::sort(ret.begin(), ret.end(), list_length_greater_than);
	for (int i = 0; i < ret.count(); ++i) {
		shuffle(ret[i]);
	}
	{
		for(const auto &lst : ret) {
			QStringList sl;
			for(auto id : lst) {
				sl << QString::number(id);
			}
			qfDebug() << "\t sorted:" << sl.join(", ");
		}
	}
	return ret;
}

QList<int> RunsWidget::runsForClass(int stage_id, int class_id, const QString &extra_where_condition, const QString &order_by)
{
	qfLogFuncFrame();
	QList<int> ret;
	for(const CompetitorForClass &cc : competitorsForClass(stage_id, class_id, extra_where_condition, order_by))
		ret << cc.runId;
	return ret;
}

QList<RunsWidget::CompetitorForClass> RunsWidget::competitorsForClass(int stage_id, int class_id, const QString &extra_where_condition, const QString &order_by)
{
	qfLogFuncFrame() << "stage:" << stage_id << "class:" << class_id;
	bool is_relays = getPlugin<EventPlugin>()->eventConfig()->isRelays();
	QList<RunsWidget::CompetitorForClass> ret;
	qf::core::sql::QueryBuilder qb;
	qb.select2("runs", "id, competitorId")
			.from("competitors")
			.joinRestricted("competitors.id", "runs.competitorId", "runs.isRunning AND runs.stageId=" QF_IARG(stage_id), "JOIN");
	if(is_relays) {
		qb.joinRestricted("runs.relayId", "relays.id", "relays.classId=" QF_IARG(class_id), "JOIN");
	}
	else {
		qb.where("competitors.classId=" QF_IARG(class_id));
	}
	if(!extra_where_condition.isEmpty())
		qb.where(extra_where_condition);
	if(!order_by.isEmpty())
		qb.orderBy(order_by);
	qfs::Query q;
	q.exec(qb.toString(), qf::core::Exception::Throw);
	while(q.next()) {
		ret << CompetitorForClass{ .competitorId = q.value("competitorId").toInt(), .runId = q.value("id").toInt() };
	}
	return ret;
}

void RunsWidget::import_start_times_ob2000()
{
	qfLogFuncFrame();
	QString fn = qf::gui::dialogs::FileDialog::getOpenFileName(this, tr("Import"));
	if(!fn.isEmpty()) {
		QFile f(fn);
		if(f.open(QFile::ReadOnly)) {
			struct Run {
				int runsId;
				QString className;
				//int startTimeMs;
			};
			try {
				qf::core::sql::Transaction transaction;
				qfs::Query q;
				QMap< QString, Run > runners;
				{
					qf::core::sql::QueryBuilder qb;
					qb.select2("runs", "id, startTimeMs")
							.select2("competitors", "registration")
							.select2("classes", "name")
							.from("competitors")
							.joinRestricted("competitors.id", "runs.competitorId", "runs.isRunning AND runs.stageId=" QF_IARG(selectedStageId()), "JOIN")
							.join("competitors.classId", "classes.id");
					q.exec(qb.toString(), qf::core::Exception::Throw);
					while(q.next()) {
						QString reg = q.value("registration").toString().toUpper().trimmed();
						if(reg.isEmpty())
							continue;
						auto &runner = runners[reg];
						runner.runsId = q.value("runs.id").toInt();
						runner.className = q.value("classes.name").toString();
					}
				}
				q.prepare("UPDATE runs SET startTimeMs=:startTimeMs WHERE id=:id");
				while(!f.atEnd()) {
					QByteArray ba = f.readLine();
					if(!ba.startsWith("    "))
						continue;
					qfDebug() << ba.size() << ba;
					//if(ba.isEmpty())
					//	break;
					QStringList sl = QString::fromLatin1(ba).split(' ', SkipEmptyParts);
					if(sl.count() < 5)
						continue;
					bool ok;
					double d = sl.value(0).toDouble(&ok);
					if(!ok) {
						qfWarning() << "Cannot convert" << sl.value(0) << "to double.";
						qfInfo() << ba;
						continue;
					}
					int st_time = static_cast<int>(d) * 60 + ((static_cast<int>(d * 100)) % 100);
					st_time *= 1000;
					QString reg = sl.value(sl.count() - 2) + sl.value(sl.count() - 1);
					reg = reg.toUpper().trimmed();
					QString class_name = sl.value(sl.count() - 3);
					if(runners.contains(reg)) {
						const auto &runner = runners[reg];
						if(runner.className == class_name) {
							q.bindValue(QStringLiteral(":id"), runner.runsId);
							q.bindValue(QStringLiteral(":startTimeMs"), st_time);
							q.exec(qf::core::Exception::Throw);
							qfDebug() << reg << "->" << QString::number(st_time / 1000 / 60) + '.' + QString::number(st_time / 1000 % 60);
						}
						else {
							qfWarning() << "Different classname" << class_name << runner.className << "start time will not be imported";
							qfInfo() << ba;
						}
					}
				}
				transaction.commit();
			}
			catch (const qf::core::Exception &e) {
				qf::gui::dialogs::MessageBox::showException(this, e);
			}
			ui->wRunsTableWidget->runsModel()->reload();
		}
	}
}

QString RunsWidget::getSaveFileName(const QString &file_name, int stage_id)
{
	QString fn = file_name;
	QString ext;
	int ix = fn.lastIndexOf('.');
	if(ix > 0)
		ext = fn.mid(ix);
	if(getPlugin<EventPlugin>()->stageCount() > 1 && stage_id > 0)
		fn = QStringLiteral("e%1-").arg(stage_id) + fn;

	fn = qfd::FileDialog::getSaveFileName(this, tr("Save as %1").arg(ext.mid(1).toUpper()), fn, '*' + ext);
	if(!fn.isEmpty()) {
		if(!fn.endsWith(ext, Qt::CaseInsensitive))
			fn += ext;
	}
	return fn;
}

void RunsWidget::export_results_stage_iofxml30()
{
	int stage_id = selectedStageId();
	QString fn = getSaveFileName(Event::RESULTS_IOFXML3_FILE, stage_id);
	if(fn.isEmpty())
		return;

	getPlugin<RunsPlugin>()->exportResultsIofXml30Stage(stage_id, fn);
}

void RunsWidget::export_results_stage_csos()
{
	int stage_id = selectedStageId();
	QString fn = getSaveFileName("results-csos.txt", stage_id);
	if(fn.isEmpty())
		return;

	getPlugin<RunsPlugin>()->exportResultsCsosStage(stage_id, fn);
}

void RunsWidget::export_results_overall_csos()
{
	QString fn = getSaveFileName("overall-results-csos.txt", 0);
	if(fn.isEmpty())
		return;
	int stage_count = getPlugin<EventPlugin>()->stageCount();
	getPlugin<RunsPlugin>()->exportResultsCsosOverall(stage_count, fn);
}

bool RunsWidget::isLockedForDrawing(int class_id, int stage_id)
{
	qf::core::sql::QueryBuilder qb;
	qb.select2("classdefs", "drawLock")
			.select2("classes", "name")
			.from("classdefs")
			.join("classdefs.classId", "classes.id")
			.where("stageId=" QF_IARG(stage_id))
			.where("classId=" QF_IARG(class_id) );
	qfs::Query q;
	QString qs = qb.toString();
	q.exec(qs, qf::core::Exception::Throw);
	if(q.next()) {
		bool is_locked = q.value("drawLock").toBool();
		return is_locked;
	}
	return false;
}

void RunsWidget::saveLockedForDrawing(int class_id, int stage_id, bool is_locked, int start_last_min)
{
	qfs::Query q;
	q.prepare("UPDATE classdefs SET drawLock=:drawLock, lastStartTimeMin=:lastStartTimeMin"
			  " WHERE stageId=" QF_IARG(stage_id) " AND classId=" QF_IARG(class_id), qf::core::Exception::Throw);
	q.bindValue(":drawLock", is_locked);
	q.bindValue(":lastStartTimeMin", start_last_min);
	q.exec(qf::core::Exception::Throw);
}

void RunsWidget::onDrawClicked()
{
	qfLogFuncFrame();
	bool is_relays = getPlugin<EventPlugin>()->eventConfig()->isRelays();
	int stage_id = selectedStageId();
	DrawMethod draw_method = DrawMethod(ui->cbxDrawMethod->currentData().toInt());
	Event::StageData stage_data = getPlugin<EventPlugin>()->stageData(stage_id);
	bool use_all_maps = stage_data.isUseAllMaps();
	qfDebug() << "DrawMethod:" << (int)draw_method << "use_all_maps:" << use_all_maps;
	QList<int> class_ids;
	int class_id = m_cbxClasses->currentData().toInt();
	if(class_id == 0) {
		if(!qf::gui::dialogs::MessageBox::askYesNo(this, tr("Draw all classes without draw lock?"), false))
			return;
		qf::core::sql::QueryBuilder qb;
		qb.select2("classdefs", "classId")
				.from("classdefs")
				.where("stageId=" QF_IARG(stage_id))
				.where("NOT drawLock");
		qfs::Query q;
		QString qs = qb.toString();
		q.exec(qs, qf::core::Exception::Throw);
		while(q.next())
			class_ids << q.value("classId").toInt();
	}
	else {
		bool is_locked = isLockedForDrawing(class_id, stage_id);
		if(is_locked) {
			qf::gui::dialogs::MessageBox::showInfo(this, tr("Class is locked for drawing."));
			return;
		}
		class_ids << class_id;
	}

	try {
		qf::core::sql::Transaction transaction;
		for(int class_id : class_ids) {
			int handicap_length_ms = getPlugin<EventPlugin>()->eventConfig()->handicapLength() * 60 * 1000;
			QVector<int> handicap_times;
			qf::core::sql::QueryBuilder qb;
			qb.select2("classdefs", "startTimeMin, startIntervalMin, vacantsBefore, vacantEvery, vacantsAfter, mapCount")
					.from("classdefs")
					.where("stageId=" QF_IARG(stage_id))
					.where("classId=" QF_IARG(class_id));
			qfs::Query q_classdefs(transaction.connection());
			q_classdefs.exec(qb.toString(), qf::core::Exception::Throw);
			if(!q_classdefs.next())
				continue;
			QList<int> runners_draw_ids;
			if(draw_method == DrawMethod::RandomNumber) {
				runners_draw_ids = runsForClass(stage_id, class_id);
				shuffle(runners_draw_ids);
			}
			else if(draw_method == DrawMethod::KeepOrder) {
				runners_draw_ids = runsForClass(stage_id, class_id, QString(), "runs.startTimeMs");
			}
			else if(draw_method == DrawMethod::RelaysFirstLeg) {
				runners_draw_ids = runsForClass(stage_id, class_id, "runs.leg=1");
			}
			else if(draw_method == DrawMethod::GroupedC) {
				QList<int> group1 = runsForClass(stage_id, class_id, "licence='C' or licence is null");
				QList<int> group2 = runsForClass(stage_id, class_id, "licence='A' or licence='B'");
				shuffle(group1);
				shuffle(group2);
				runners_draw_ids = group1 + group2;
			}
			else if(draw_method == DrawMethod::GroupedCB) {
				QList<int> group1 = runsForClass(stage_id, class_id, "licence='C' or licence is null");
				QList<int> group2 = runsForClass(stage_id, class_id, "licence='B'");
				QList<int> group3 = runsForClass(stage_id, class_id, "licence='A' or licence='R' or licence='E'");
				shuffle(group1);
				shuffle(group2);
				shuffle(group3);
				runners_draw_ids = group1 + group2 + group3;
			}
			else if(draw_method == DrawMethod::GroupedRanking) {
				QList<int> group1 = runsForClass(stage_id, class_id, "ranking>300 or ranking is null");
				QList<int> group2 = runsForClass(stage_id, class_id, "ranking>100 and ranking<=300");
				QList<int> group3 = runsForClass(stage_id, class_id, "ranking<=100");
				shuffle(group1);
				shuffle(group2);
				shuffle(group3);
				runners_draw_ids = group1 + group2 + group3;
			}
			else if(draw_method == DrawMethod::Handicap) {
				int stage_count = getPlugin<EventPlugin>()->eventConfig()->stageCount();
				qf::core::utils::Table results = getPlugin<RunsPlugin>()->nstagesClassResultsTable(stage_count - 1, class_id);
				QMap<int, int> competitor_to_run;
				for(const auto &cc : competitorsForClass(stage_count, class_id))
					competitor_to_run[cc.competitorId] = cc.runId;
				//int n = 0;
				for (int i = 0; i < results.rowCount(); ++i) {
					qf::core::utils::TableRow r = results.row(i);
					int run_id = competitor_to_run.take(r.value("competitors.id").toInt());
					if(run_id > 0) {
						runners_draw_ids << run_id;
						int loss_ms = r.value("timeLossMs").toInt();
						if(loss_ms < handicap_length_ms) {
							qfDebug() << "loss:" << loss_ms;
							handicap_times << loss_ms;
						}
					}
				}
				runners_draw_ids << competitor_to_run.values();
			}
			else if(draw_method == DrawMethod::StageReverseOrder) {
				QMap<int, int> competitor_to_run;
				for(const auto &cc : competitorsForClass(stage_id, class_id))
					competitor_to_run[cc.competitorId] = cc.runId;
				qf::core::sql::QueryBuilder qb1;
				qb1.select2("runs", "competitorId")
						//.select2("competitors", "lastName")
						.from("competitors")
						.joinRestricted("competitors.id", "runs.competitorId", "runs.stageId=" QF_IARG(1), "JOIN")
						.where("competitors.classId=" QF_IARG(class_id))
						.orderBy("runs.startTimeMs DESC");
				qfs::Query q(transaction.connection());
				q.exec(qb1.toString(), qf::core::Exception::Throw);
				while(q.next()) {
					int competitor_id = q.value("competitorId").toInt();
					if(competitor_to_run.contains(competitor_id)) {
						//qfDebug() << "\t adding to poll";
						runners_draw_ids << competitor_to_run.take(competitor_id);
					}
				}
				runners_draw_ids << competitor_to_run.values();
			}
			else if(draw_method == DrawMethod::EquidistantClubs || draw_method == DrawMethod::RandomizedEquidistantClubs) {
				QMap<int, QString> runner_id_to_club;
				QList< QList<int> > runners_by_club = runnersByClubSortedByCount(stage_id, class_id, runner_id_to_club);
				if(runners_by_club.count()) {
					int runners_cnt = 0;
					for(const auto &lst : runners_by_club) {
						runners_cnt += lst.count();
					}
					runners_draw_ids.reserve(runners_cnt);
					runners_draw_ids << runners_by_club.first();
					int club_ix = 1;
					int slot_len = 1;
					int insert_ix = slot_len;
					while(club_ix < runners_by_club.count()) {
						QList<int> &club_runners = runners_by_club[club_ix];
						QF_ASSERT(club_runners.count() > 0, "Club runners cannot be empty in this phase!", break);
						int runner_id = club_runners.takeFirst();
						qfDebug() << "inserting id:" << runner_id << "at index:" << insert_ix;
						runners_draw_ids.insert(insert_ix, runner_id);

						// set club index for next scan
						if(club_runners.isEmpty()) {
							// take next club
							club_ix++;
							qfDebug() << "club_ix:" << club_ix;
						}

						// set insert index for next scan
						insert_ix += slot_len + 1;
						if(insert_ix > runners_draw_ids.count()) {
							slot_len++;
							insert_ix = slot_len;
						}
					}
				}
				if(draw_method == DrawMethod::RandomizedEquidistantClubs) {
					QRandomGenerator *rnd_gen = QRandomGenerator::global();
					//rnd_gen->seed((uint)QTime::currentTime().msecsSinceStartOfDay());
					//qsrand((uint)QTime::currentTime().msecsSinceStartOfDay());
					int cnt = runners_draw_ids.count();
					for (int i = 0; i < 2*cnt; ++i) {
						// randomly switch rudders fi their clubs will not get consequent
						int ix1 = rnd_gen->bounded(cnt);
						//int ix1 = (int)(qrand() * (double)cnt / RAND_MAX);
						if(ix1 >= cnt)
							ix1 = cnt - 1;
						int ix2 = rnd_gen->bounded(cnt);
						//int ix2 = (int)(qrand() * (double)cnt / RAND_MAX);
						if(ix2 >= cnt)
							ix2 = cnt - 1;
						if(ix1 == ix2)
							continue;
						if((ix1 - ix2) == 1 || (ix2 - ix1) == 1)
							continue;
						QString club1 = runner_id_to_club.value(runners_draw_ids[ix1]);
						QString club2 = runner_id_to_club.value(runners_draw_ids[ix2]);
						if(ix1 > 0) {
							QString club = runner_id_to_club.value(runners_draw_ids[ix1 - 1]);
							if(club == club2)
								continue;
						}
						if(ix1 < cnt - 1) {
							QString club = runner_id_to_club.value(runners_draw_ids[ix1 + 1]);
							if(club == club2)
								continue;
						}
						if(ix2 > 0) {
							QString club = runner_id_to_club.value(runners_draw_ids[ix2 - 1]);
							if(club == club1)
								continue;
						}
						if(ix2 < cnt - 1) {
							QString club = runner_id_to_club.value(runners_draw_ids[ix2 + 1]);
							if(club == club1)
								continue;
						}
						// can switch
						int id = runners_draw_ids[ix1];
						runners_draw_ids[ix1] = runners_draw_ids[ix2];
						runners_draw_ids[ix2] = id;
					}
				}
			}
			if(runners_draw_ids.count()) {
				// save drawing to SQL
				int interval = q_classdefs.value("startIntervalMin").toInt() * 60 * 1000;
				if(interval == 0 && !is_relays) {
					if(!qf::gui::dialogs::MessageBox::askYesNo(this, tr("Start interval is zero, proceed anyway?"), false))
						continue;
				}
				int start0 = q_classdefs.value("startTimeMin").toInt() * 60 * 1000;
				int vacants_before = q_classdefs.value("vacantsBefore").toInt();
				int vacant_every = q_classdefs.value("vacantEvery").toInt();
				int vacants_after = q_classdefs.value("vacantsAfter").toInt();
				int map_count = q_classdefs.value("mapCount").toInt();
				int start = start0;
				int maps_used = 0;

				if(map_count <= 0)
					use_all_maps = false;

				if(draw_method != DrawMethod::Handicap) {
					start += vacants_before * interval;
					if(use_all_maps) {
						maps_used += vacants_before;
						int spare_map_count = (map_count - maps_used - vacants_after - runners_draw_ids.count());
						if(spare_map_count > 0)
							vacant_every = runners_draw_ids.count() / spare_map_count;
						else
							vacant_every = 0;
					}
				}
				qfs::Query q(transaction.connection());
				q.prepare("UPDATE runs SET startTimeMs=:startTimeMs WHERE id=:id");
				qfDebug() << "draw runners count:" << runners_draw_ids.count();
				for (int i = 0; i < runners_draw_ids.count(); ++i) {
					int runner_id = runners_draw_ids.at(i);
					q.bindValue(QStringLiteral(":id"), runner_id);
					if(draw_method == DrawMethod::Handicap) {
						if(handicap_times.isEmpty()) {
							++maps_used;
							start = start0 + handicap_length_ms + maps_used * interval;
						}
						else {
							start = start0 + handicap_times.takeFirst();
						}
						q.bindValue(QStringLiteral(":startTimeMs"), start);
						q.exec(qf::core::Exception::Throw);
					}
					else {
						qfDebug() << i << "runner id:" << runner_id << "start:" << (start / 60 / 1000);
						q.bindValue(QStringLiteral(":startTimeMs"), start);
						q.exec(qf::core::Exception::Throw);
						start += interval;
						maps_used++;
						int runners_left = runners_draw_ids.count() - i - 1;
						bool can_add_vacant = use_all_maps?
									map_count > (maps_used + runners_left + vacants_after):
									true;
						if(can_add_vacant && vacant_every > 0 && (((i + 1) % vacant_every) == 0)) {
							//qfInfo() << i << vacant_every << ((i+1) % vacant_every);
							qfDebug() << "vakant" << i << (start / 60 / 1000);
							start += interval;
							maps_used++;
						}
					}
				}
				if(use_all_maps)
					vacants_after = map_count - maps_used;
				if(vacants_after > 0)
					start += vacants_after * interval;
				saveLockedForDrawing(class_id, stage_id, true, (start - interval) / 60 / 1000);
			}
		}
		transaction.commit();
	}
	catch (const qf::core::Exception &e) {
		qf::gui::dialogs::MessageBox::showException(this, e);
	}
	ui->wRunsTableWidget->reload();
}

void RunsWidget::onDrawRemoveClicked()
{
	int class_id = m_cbxClasses->currentData().toInt();
	if(class_id == 0)
		return;
	if(!qf::gui::dialogs::MessageBox::askYesNo(this, tr("Reset all start times and unlock drawing for this class?"), false))
		return;
	try {
		auto *runs_model = ui->wRunsTableWidget->runsModel();
		qf::core::sql::Transaction transaction(qfs::Connection::forName());
		for (int i = 0; i < runs_model->rowCount(); ++i) {
			runs_model->setValue(i, "startTimeMs", QVariant());
			// bypass mid-air collision check
			runs_model->quickevent::gui::og::SqlTableModel::postRow(i, qf::core::Exception::Throw);
		}
		int stage_id = selectedStageId();
		saveLockedForDrawing(class_id, stage_id, false, 0);
		transaction.commit();
		runs_model->reload();
	}
	catch (const qf::core::Exception &e) {
		qf::gui::dialogs::MessageBox::showException(this, e);
	}
}

void RunsWidget::onCbxStageCurrentIndexChanged()
{
	getPlugin<RunsPlugin>()->setSelectedStageId(m_cbxStage->currentData().toInt());
	reload();
	qfInfo() << "selected stage:" << selectedStageId();
}

int RunsWidget::selectedStageId()
{
	return getPlugin<RunsPlugin>()->selectedStageId();
}

void RunsWidget::export_startList_stage_iofxml30()
{
	int stage_id = selectedStageId();
	QString fn = getSaveFileName(Event::START_LIST_IOFXML3_FILE, stage_id);
	if(fn.isEmpty())
		return;
	getPlugin<RunsPlugin>()->exportStartListStageIofXml30(stage_id, fn);
}


void RunsWidget::export_startList_stage_csv_sime()
{
	qff::MainWindow *fwk = qff::MainWindow::frameWork();
	quickevent::gui::ReportOptionsDialog dlg(fwk);
	dlg.setPersistentSettingsId("startListCsvSimeReportOptions");
	dlg.loadPersistentSettings();
	dlg.setStartListOptionsVisible(true);
	dlg.setVacantsVisible(false);
	dlg.setPageLayoutVisible(false);
	dlg.setStartTimeFormatVisible(false);
	dlg.setColumnCountEnable(false);
	if(dlg.exec()) {
		QString fn = getSaveFileName("startlist.csv", selectedStageId());
		if(fn.isEmpty())
			return;
		getPlugin<RunsPlugin>()->exportStartListCurrentStageCsvSime(fn, dlg.isStartListPrintStartNumbers(), dlg.sqlWhereExpression());
	}
}

void RunsWidget::export_startList_stage_tv_graphics()
{
	QString fn = getSaveFileName("startlist_tv.csv", selectedStageId());
	if(fn.isEmpty())
		return;
	getPlugin<RunsPlugin>()->exportStartListCurrentStageTvGraphics(fn);
}

void RunsWidget::export_results_stage_csv()
{
	QString fn = getSaveFileName("results.csv", selectedStageId());
	if(fn.isEmpty())
		return;

	bool is_iof_race = getPlugin<EventPlugin>()->eventConfig()->isIofRace();
	quickevent::core::exporters::StageResultsCsvExporter exp(is_iof_race);
	QFileInfo fi(fn);
	exp.setOutDir(fi.absolutePath());
	exp.setOutFile(fi.fileName());
	exp.setSimplePath(true);
	exp.setWithDidNotStart(true);
	exp.generateCsvSingle();
}

void RunsWidget::editCompetitors(int mode)
{
	if(mode == qfm::DataDocument::ModeDelete) {
		auto *tv = ui->wRunsTableWidget->tableView();
		QList<int> sel_rows = tv->selectedRowsIndexes();
		if(sel_rows.count() <= 1)
			return;
		if(qfd::MessageBox::askYesNo(this, tr("Really delete all the selected competitors? This action cannot be reverted."), false)) {
			qfs::Transaction transaction;
			int n = 0;
			for(int ix : sel_rows) {
				int id = tv->tableRow(ix).value(tv->tableModel()->idColumnName()).toInt();
				if(id > 0) {
					Competitors::CompetitorDocument doc;
					doc.load(id, qfm::DataDocument::ModeDelete);
					doc.drop();
					n++;
				}
			}
			if(n > 0) {
				if(qfd::MessageBox::askYesNo(this, tr("Confirm deletion of %1 competitors.").arg(n), false)) {
					transaction.commit();
					tv->reload();
				}
				else {
					transaction.rollback();
				}
			}
		}
	}
}

void RunsWidget::editCompetitorOnPunch(int siid)
{
	qfs::Query q;
	q.exec("SELECT id FROM competitors WHERE siId=" + QString::number(siid), qfc::Exception::Throw);
	if(q.next()) {
		int competitor_id = q.value(0).toInt();
		if(competitor_id > 0) {
			editCompetitor_helper(competitor_id, qfm::DataDocument::ModeEdit, 0);
		}
	}
	else {
		editCompetitor_helper(QVariant(), qfm::DataDocument::ModeInsert, siid);
	}
}

void RunsWidget::report_competitorsStatistics()
{
	qfLogFuncFrame();
	int stage_cnt = getPlugin<EventPlugin>()->stageCount();

	qfs::QueryBuilder qb;
	qb.select2("classes", "id, name").from("classes").orderBy("classes.name");
	qf::gui::model::SqlTableModel m;
	m.setQueryBuilder(qb);
	m.reload();
	qfu::TreeTable tt = m.toTreeTable();
	tt.setValue("event", getPlugin<EventPlugin>()->eventConfig()->value("event"));
	for (int stage_id = 1; stage_id <= stage_cnt; ++stage_id) {
		QString prefix = "e" + QString::number(stage_id) + "_";
		QString col_runs_count = prefix + "runCount";
		QString col_map_count = prefix + "mapCount";
		tt.appendColumn(col_runs_count, QMetaType(QMetaType::Int));
		tt.appendColumn(col_map_count, QMetaType(QMetaType::Int));
		{
			qfs::QueryBuilder qb;
			qb.select2("classes", "name")
				.select("COUNT(runs.id) AS runCount")
				.select("0 as mapCount") // classdefs.mapCount must be in any agregation function in PSQL, MIN can be here as well
				.from("classes")
				.joinRestricted("classes.id", "classdefs.classid", "classdefs.stageId={{stage_id}}")
				.join("classes.id", "competitors.classId")
				.joinRestricted("competitors.id", "runs.competitorId", "runs.isRunning AND runs.stageId={{stage_id}}")
				.groupBy("classes.name")
				.orderBy("classes.name");
			QVariantMap qpm;
			qpm["stage_id"] = stage_id;
			qfs::Query q;
			q.execThrow(qb.toString(qpm));
			int i = 0;
			while(q.next()) {
				qf::core::utils::TreeTableRow tt_row = tt.row(i);
				tt_row.setValue(col_runs_count, q.value("runCount"));
				tt_row.setValue(col_map_count, q.value("mapCount"));
				tt.setRow(i, tt_row);
				i++;
			}
		}
	}
	qfDebug() << tt.toString();
	QVariantMap props;
	//props["isBreakAfterEachClass"] = (opts.breakType() != (int)quickevent::gui::ReportOptionsDialog::BreakType::None);
	//props["isColumnBreak"] = (opts.breakType() == (int)quickevent::gui::ReportOptionsDialog::BreakType::Column);
	props["stageCount"] = stage_cnt;
	QString rep_fn = getPlugin<RunsPlugin>()->findReportFile("competitorsStatistics.qml");
	qf::gui::reports::ReportViewWidget::showReport(this
								, rep_fn
								, tt.toVariant()
								, tr("Competitors statistics")
								, "competitorsStatistics"
								, props
								);
}

void RunsWidget::editCompetitor_helper(const QVariant &id, int mode, int siid)
{
	qfLogFuncFrame() << "id:" << id << "mode:" << mode;
	bool ok = false;
	bool save_and_next = false;

	{
		auto *w = new CompetitorWidget();
		w->setWindowTitle(tr("Edit Competitor"));
		qfd::Dialog dlg(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
		dlg.setDefaultButton(QDialogButtonBox::Ok);
		if(mode == qf::gui::model::DataDocument::ModeInsert || mode == qf::gui::model::DataDocument::ModeEdit) {
			QPushButton *bt_save_and_next = dlg.buttonBox()->addButton(tr("Ok and &next"), QDialogButtonBox::ActionRole);
			connect(dlg.buttonBox(), &qf::gui::DialogButtonBox::clicked, &dlg, [&save_and_next, bt_save_and_next](QAbstractButton *button) {
				save_and_next = (button == bt_save_and_next);
			});
		}
		dlg.setCentralWidget(w);
		w->load(id, mode);
		auto *doc = qobject_cast<Competitors::CompetitorDocument*>(w->dataController()->document());
		QF_ASSERT(doc != nullptr, "Document is null!", return);
		if(mode == qfm::DataDocument::ModeInsert) {
			if(siid == 0) {
				bool is_relays = getPlugin<EventPlugin>()->eventConfig()->isRelays();
				if(!is_relays) {
					int class_id = m_cbxClasses->currentData().toInt();
					if(class_id > 0)
						doc->setValue("competitors.classId", class_id);
					else
						doc->setValue("competitors.classId", QVariant());
				}
			}
			else {
				w->loadFromRegistrations(siid);
			}
		}
		connect(doc, &Competitors::CompetitorDocument::saved, this, [this, w](const QVariant &, int mode) {
			if (auto run_id = w->runId(selectedStageId()); run_id.has_value()) {
				ui->wRunsTableWidget->tableView()->rowExternallySaved(run_id.value(), mode);
			}
		});
		ok = dlg.exec();

	}
	if(ok && save_and_next) {
		QTimer::singleShot(0, this, [this]() {
			this->editCompetitor(QVariant(), qf::gui::model::DataDocument::ModeInsert);
		});
	}
}
