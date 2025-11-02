#include "eventstatisticswidget.h"
#include "ui_eventstatisticswidget.h"

#include "eventstatisticsoptions.h"
#include "runsplugin.h"

#include <plugins/Event/src/eventplugin.h>

#include <quickevent/gui/reportoptionsdialog.h>

#include <quickevent/gui/og/sqltablemodel.h>
#include <quickevent/core/og/timems.h>

#include <qf/gui/framework/mainwindow.h>
#include <qf/gui/reports/widgets/reportviewwidget.h>
#include <qf/gui/style.h>
#include <qf/gui/log.h>

#include <qf/core/sql/querybuilder.h>
#include <qf/core/assert.h>
#include <qf/core/sql/query.h>

#include <QElapsedTimer>
#include <QSettings>
#include <QTimer>
#include <QScrollBar>
#include <QPushButton>

using qf::gui::framework::getPlugin;
using Event::EventPlugin;
using Runs::RunsPlugin;

//============================================================
// EventStatisticsModel
//============================================================
class EventStatisticsModel : public quickevent::gui::og::SqlTableModel
{
	Q_DECLARE_TR_FUNCTIONS(EventStatisticsModel)
private:
	typedef quickevent::gui::og::SqlTableModel Super;
public:
	enum Cols {
		col_className,
		col_mapCount,
		col_freeMapCount,
		col_runnersCount,
		col_startFirstMs,
		col_startLastMs,
		col_time1Ms,
		col_time3Ms,
		col_timeToCloseMs,
		col_runnersFinished,
		col_runnersNotFinished,
		col_resultsNotPrinted,
		col_resultsNotPrintedMs,
		col_COUNT
	};
public:
	EventStatisticsModel(QObject *parent);

	QVariant data(const QModelIndex &index, int role) const Q_DECL_OVERRIDE;
	QVariant value(int row_ix, int column_ix) const Q_DECL_OVERRIDE;
};

EventStatisticsModel::EventStatisticsModel(QObject *parent)
	: Super(parent)
{
	clearColumns(col_COUNT);
	setColumn(col_className, ColumnDefinition("classes.name", tr("Class")));
	setColumn(col_mapCount, ColumnDefinition("classdefs.mapCount", tr("Maps")));
	setColumn(col_freeMapCount, ColumnDefinition("freeMapCount", tr("Free maps")));
	setColumn(col_runnersCount, ColumnDefinition("runnersCount", tr("Runners")));
	setColumn(col_startFirstMs, ColumnDefinition("startFirstMs", tr("Start first")).setCastType(qMetaTypeId<quickevent::core::og::TimeMs>()));
	setColumn(col_startLastMs, ColumnDefinition("startLastMs", tr("Start last")).setCastType(qMetaTypeId<quickevent::core::og::TimeMs>()));
	setColumn(col_time1Ms, ColumnDefinition("time1Ms", tr("1st time")).setToolTip(tr("Finish time of first runner in current class.")).setCastType(qMetaTypeId<quickevent::core::og::TimeMs>()));
	setColumn(col_time3Ms, ColumnDefinition("time3Ms", tr("3rd time")).setToolTip(tr("Finish time of third runner in current class.")).setCastType(qMetaTypeId<quickevent::core::og::TimeMs>()));
	setColumn(col_timeToCloseMs, ColumnDefinition("timeToCloseMs", tr("Time to close"))
			  .setToolTip(tr("Time until new finished competitors should not affect standings on first three places."))
			  .setCastType(qMetaTypeId<quickevent::core::og::TimeMs>()));
	setColumn(col_runnersFinished, ColumnDefinition("runnersFinished", tr("Finished")));
	setColumn(col_runnersNotFinished, ColumnDefinition("runnersNotFinished", tr("Not finished")));
	setColumn(col_resultsNotPrinted, ColumnDefinition("resultsNotPrinted", tr("New results")).setToolTip(tr("Number of finished competitors not printed in results.")));
	setColumn(col_resultsNotPrintedMs, ColumnDefinition("resultsNotPrintedMs", tr("Not printed time"))
			  .setToolTip(tr("Time since recent results printout."))
			  .setCastType(qMetaTypeId<quickevent::core::og::TimeMs>())
			  );
	{
		static const auto competiting_cond = QStringLiteral("runs.stageId={{stage_id}} AND runs.isRunning AND competitors.classId=classes.id");
		qf::core::sql::QueryBuilder qb_runners_count;
		qb_runners_count.select("COUNT(runs.id)")
				.from("runs").joinRestricted("runs.competitorId", "competitors.id", competiting_cond, qf::core::sql::QueryBuilder::INNER_JOIN);
		qf::core::sql::QueryBuilder qb_runners_finished;
		qb_runners_finished.select("COUNT(runs.id)")
				.from("runs").joinRestricted("runs.competitorId", "competitors.id", competiting_cond, qf::core::sql::QueryBuilder::INNER_JOIN)
				.where("runs.finishTimeMs > 0 OR runs.disqualified");
		qf::core::sql::QueryBuilder qb_runners_start_first;
		qb_runners_start_first.select("MIN(runs.startTimeMs)")
				.from("runs").joinRestricted("runs.competitorId", "competitors.id", competiting_cond, qf::core::sql::QueryBuilder::INNER_JOIN)
				.where("runs.startTimeMs IS NOT NULL");
		qf::core::sql::QueryBuilder qb_runners_start_last;
		qb_runners_start_last.select("MAX(runs.startTimeMs)")
				.from("runs").joinRestricted("runs.competitorId", "competitors.id", competiting_cond, qf::core::sql::QueryBuilder::INNER_JOIN)
				.where("runs.startTimeMs IS NOT NULL");
		static const auto in_results_cond = competiting_cond + QStringLiteral(" AND runs.timeMs>0 AND NOT runs.disqualified");
		qf::core::sql::QueryBuilder qb_first_time;
		{
			qf::core::sql::QueryBuilder qb;
			qb.select("runs.timeMs")
				.from("runs").joinRestricted("runs.competitorId", "competitors.id", in_results_cond, qf::core::sql::QueryBuilder::INNER_JOIN)
				.orderBy("runs.timeMs")
				.limit(1)
				.as("results1");
			qb_first_time.select("MAX(timeMs)")
					.from(qb.toString());
		}
		qf::core::sql::QueryBuilder qb_third_time;
		{
			qf::core::sql::QueryBuilder qb;
			qb.select("runs.timeMs")
				.from("runs").joinRestricted("runs.competitorId", "competitors.id", in_results_cond, qf::core::sql::QueryBuilder::INNER_JOIN)
				.orderBy("runs.timeMs")
				.limit(3)
				.as("results3");
			qb_third_time.select("CASE WHEN COUNT(timeMs) = 3 THEN MAX(timeMs) ELSE NULL END")
					.from(qb.toString());
		}

		qf::core::sql::QueryBuilder qb;
		qb.select2("classes", "*")
				.select2("classdefs", "*")
				.select("(" + qb_runners_count.toString() + ") AS runnersCount")
				.select("(" + qb_runners_finished.toString() + ") AS runnersFinished")
				.select("(" + qb_runners_start_first.toString() + ") AS startFirstMs")
				.select("(" + qb_runners_start_last.toString() + ") AS startLastMs")
				.select("(" + qb_first_time.toString() + ") AS time1Ms")
				.select("(" + qb_third_time.toString() + ") AS time3Ms")
				.select("0 AS timeToCloseMs")
				.select("0 AS freeMapCount")
				.select("0 AS runnersNotFinished")
				.select("0 AS resultsNotPrinted")
				.select("0 AS resultsNotPrintedMs")
				.from("classes")
				.joinRestricted("classes.id", "classdefs.classId", "classdefs.stageId={{stage_id}}")
				//.join("classes.id", "competitors.classId")
				//.joinRestricted("competitors.id", "runs.competitorId", "runs.stageId={{stage_id}}")
				//.groupBy("classes.id")
				.orderBy("classes.name");
		setQueryBuilder(qb, false);
	}
}

QVariant EventStatisticsModel::value(int row_ix, int column_ix) const
{
	if(column_ix == col_freeMapCount) {
		int cnt = value(row_ix, col_mapCount).toInt() - value(row_ix, col_runnersCount).toInt();
		return cnt;
	}
	if(column_ix == col_timeToCloseMs) {
		int stage_start_msec = getPlugin<EventPlugin>()->stageStartMsec(getPlugin<EventPlugin>()->currentStageId());
		int curr_time_msec = QTime::currentTime().msecsSinceStartOfDay();
		int runners_count = value(row_ix, col_runnersCount).toInt();
		int runners_finished = value(row_ix, col_runnersFinished).toInt();
		if(runners_count == runners_finished) {
			return 0;
		}
		int time3_msec = value(row_ix, col_time3Ms).toInt();
		if(time3_msec == 0) {
			return QVariant();
		}
		int start_last_msec = value(row_ix, col_startLastMs).toInt();
		int time_to_close_msec = stage_start_msec + start_last_msec + time3_msec - curr_time_msec;
		return time_to_close_msec;
	}
	if(column_ix == col_runnersNotFinished) {
		int cnt = value(row_ix, col_runnersCount).toInt() - value(row_ix, col_runnersFinished).toInt();
		return cnt;
	}
	if(column_ix == col_resultsNotPrinted) {
		int results_count = tableRow(row_ix).value(QStringLiteral("resultsCount")).toInt();
		int cnt = value(row_ix, col_runnersFinished).toInt() - results_count;
		return cnt;
	}
	if(column_ix == col_resultsNotPrintedMs) {
		QDateTime dt1 = tableRow(row_ix).value(QStringLiteral("resultsPrintTS")).toDateTime();
		QDateTime dt2 = QDateTime::currentDateTime();
		if(!dt1.isValid())
			return QVariant{QMetaType(QMetaType::Type::Int)};
		int time_since_last_printed_msec = dt1.msecsTo(dt2);
		return time_since_last_printed_msec;
	}
	return Super::value(row_ix, column_ix);
}

QVariant EventStatisticsModel::data(const QModelIndex &index, int role) const
{
	if(!getPlugin<EventPlugin>()->isEventOpen())
		return QVariant();
	int col = index.column();
	if(role == Qt::BackgroundRole) {
		if(col == col_freeMapCount) {
			int cnt = data(index, Qt::DisplayRole).toInt();
			if(cnt < 0)
				return QColor(Qt::red);
			return QVariant();
		}
	}
	return Super::data(index, role);
}

//============================================================
// FooterModel
//============================================================
class FooterModel : public QAbstractTableModel
{
	typedef QAbstractTableModel Super;
public:
	FooterModel(QObject *parent);

	int rowCount(const QModelIndex &parent = QModelIndex()) const Q_DECL_OVERRIDE;
	int columnCount(const QModelIndex &parent = QModelIndex()) const Q_DECL_OVERRIDE;
	QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const Q_DECL_OVERRIDE;
	QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const Q_DECL_OVERRIDE;

	qf::gui::model::TableModel* masterModel() const;
	void setMasterModel(qf::gui::model::TableModel *masterModel);

	void reload();
private:
	qf::gui::model::TableModel *m_masterModel = nullptr;
	QVector<QVariant> m_columnSums;
};

FooterModel::FooterModel(QObject *parent)
	: Super(parent)
{
}

int FooterModel::rowCount(const QModelIndex &parent) const
{
	Q_UNUSED(parent)
	return 0;
}

int FooterModel::columnCount(const QModelIndex &parent) const
{
	return masterModel()->columnCount(parent);
}

QVariant FooterModel::data(const QModelIndex &index, int role) const
{
	Q_UNUSED(index)
	Q_UNUSED(role)
	return QVariant();
}

QVariant FooterModel::headerData(int section, Qt::Orientation orientation, int role) const
{
	if(orientation == Qt::Horizontal) {
		if(role == Qt::DisplayRole) {
			return m_columnSums.value(section);
		}
		if(role == Qt::TextAlignmentRole) {
			return Qt::AlignRight;
		}
	}
	return Super::headerData(section, orientation, role);
}

qf::gui::model::TableModel *FooterModel::masterModel() const
{
	QF_ASSERT_EX(m_masterModel != nullptr, "Master model is NULL");
	return m_masterModel;
}

void FooterModel::setMasterModel(qf::gui::model::TableModel *masterModel)
{
	m_masterModel = masterModel;
	connect(m_masterModel, &qf::gui::model::SqlTableModel::reloaded, this, &FooterModel::reload);
}

void FooterModel::reload()
{
	qfLogFuncFrame();
	auto *mm = masterModel();
	m_columnSums.resize(columnCount());
	for (int i = 0; i < columnCount(); ++i) {
		int type = mm->columnType(i);
		//qfInfo() << type << "type:" << QVariant::typeToName(type);
		int isum = 0;
		double dsum = 0;
		bool int_col = (type == QMetaType::Type::Int || type == QMetaType::Type::UInt || type == QMetaType::Type::LongLong || type == QMetaType::Type::ULongLong);
		bool double_col = (type == QMetaType::Type::Double);
		for (int j = 0; j < mm->rowCount(); ++j) {
			if(int_col) {
				isum += mm->value(j, i).toInt();
			}
			else if(double_col) {
				dsum += mm->value(j, i).toDouble();
			}
		}
		if(int_col)
			m_columnSums[i] = isum;
		else if(double_col)
			m_columnSums[i] = dsum;
		else
			m_columnSums[i] = QVariant();
		if(i == 2)
			qfDebug() << "SUM:" << m_columnSums[i];
	}
	emit headerDataChanged(Qt::Horizontal, 0, columnCount() - 1);
}

//============================================================
// FooterView
//============================================================
class FooterView : public QHeaderView
{
	typedef QHeaderView Super;
public:
	FooterView(QTableView *table_view, QWidget *parent = nullptr);
	~FooterView() override;

	void syncSectionSizes();
private:
	void resetFooterAttributes();
private:
	QTableView *m_tableView;
};

FooterView::FooterView(QTableView *table_view, QWidget *parent)
	: Super(Qt::Horizontal, parent)
	, m_tableView(table_view)
{
	resetFooterAttributes();
	//QHeaderView *vh = table_view->verticalHeader();
	QHeaderView *hh = table_view->horizontalHeader();
	if(hh) {
		//qfWarning() << "header view:" << hh->metaObject()->className();
		//setMaximumHeight(hh->defaultSectionSize());
		connect(hh, &QHeaderView::sectionResized, this, [this](int logical_index, int old_size, int new_size) {
			Q_UNUSED(old_size)
			this->resizeSection(logical_index, new_size);
		});
		connect(hh, &QHeaderView::sectionMoved, this, [this](int logical_index, int old_visual_index, int new_visual_index) {
			Q_UNUSED(logical_index)
			this->moveSection(old_visual_index, new_visual_index);
		});
	}
	QScrollBar *sb = table_view->horizontalScrollBar();
	if(sb) {
		connect(sb, &QScrollBar::valueChanged, this, [this]() {
			this->setOffset(m_tableView->horizontalHeader()->offset());
		});
	}
}

FooterView::~FooterView() = default;

void FooterView::resetFooterAttributes()
{
	setSectionResizeMode(QHeaderView::Fixed);
	setSortIndicatorShown(false);
}

void FooterView::syncSectionSizes()
{
	//qfInfo() << Q_FUNC_INFO << this;
	QAbstractItemModel *m = model();
	QF_ASSERT(m != nullptr, "Model is NULL!", return);

	QHeaderView *vh = m_tableView->verticalHeader();
	if(vh) {
		int w = vh->width();
		setViewportMargins(w, 0, 0, 0);
	}
	QHeaderView *hh = m_tableView->horizontalHeader();
	if(hh) {
		setMinimumHeight(hh->height());
		setMaximumHeight(hh->height());

		QByteArray ba = hh->saveState();
		restoreState(ba);

		resetFooterAttributes();
	}
}

//============================================================
// EventStatisticsWidget
//============================================================
EventStatisticsWidget::EventStatisticsWidget(QWidget *parent)
	: QWidget(parent)
	, ui(new Ui::EventStatisticsWidget)
{
	ui->setupUi(this);

	ui->btClearNewInSelectedRows->setIcon(qf::gui::Style::icon("clear"));
	ui->btPrintResultsSelected->setIcon(qf::gui::Style::icon("print-selected"));
	ui->btPrintResultsNew->setIcon(qf::gui::Style::icon("print"));
	ui->btOptions->setIcon(qf::gui::Style::icon("settings"));
	ui->btReload->setIcon(qf::gui::Style::icon("reload"));

	ui->tableView->setPersistentSettingsId("tblEventStatistics");
	ui->tableView->setReadOnly(true);

	m_tableFooterView = new FooterView(ui->tableView);
	m_tableFooterView->setMinimumHeight(20);
	ui->tableLayout->addWidget(m_tableFooterView);

	connect(getPlugin<EventPlugin>(), &Event::EventPlugin::dbEventNotify, this, &EventStatisticsWidget::onDbEventNotify, Qt::QueuedConnection);
	connect(getPlugin<EventPlugin>(), &Event::EventPlugin::currentStageIdChanged, this, &EventStatisticsWidget::reloadDeferred);

	connect(ui->btReload, &QPushButton::clicked, this, &EventStatisticsWidget::onReloadClicked);
	connect(ui->btOptions, &QPushButton::clicked, this, &EventStatisticsWidget::onOptionsClicked);
	connect(ui->btPrintResultsSelected, &QPushButton::clicked, this, &EventStatisticsWidget::onPrintResultsSelectedClicked);
	connect(ui->btPrintResultsNew, &QPushButton::clicked, this, &EventStatisticsWidget::onPrintResultsNewClicked);
	connect(ui->btClearNewInSelectedRows, &QPushButton::clicked, this, &EventStatisticsWidget::onClearNewInSelectedRowsClicked);

	initAutoRefreshTimer();
}

EventStatisticsWidget::~EventStatisticsWidget()
{
	delete ui;
}

void EventStatisticsWidget::reloadDeferred()
{
	if(!isVisible())
		return;

	if(!m_reloadLaterTimer) {
		m_reloadLaterTimer = new QTimer(this);
		m_reloadLaterTimer->setInterval(200);
		m_reloadLaterTimer->setSingleShot(true);
		connect(m_reloadLaterTimer, &QTimer::timeout, this, &EventStatisticsWidget::reload);
	}
	if(!m_reloadLaterTimer->isActive())
		m_reloadLaterTimer->start();
}

void EventStatisticsWidget::reload()
{
	qfLogFuncFrame() << isVisible();
	if(!isVisible())
		return;

	int stage_id = currentStageId();
	if(stage_id == 0)
		return;

	if(!m_tableModel) {
		m_tableModel = new EventStatisticsModel(this);
		m_tableFooterModel = new FooterModel(this);
		m_tableFooterModel->setMasterModel(m_tableModel);

		ui->tableView->setTableModel(m_tableModel);
		ui->tableView->horizontalHeader()->resizeSections(QHeaderView::ResizeToContents);
		m_tableFooterView->setModel(m_tableFooterModel);
		// we must load column size & position here, since header columns didn't exist
		// when loadPersistentSettingsRecursively was called by framework
		ui->tableView->loadPersistentSettings();
	}

	if(!getPlugin<EventPlugin>()->isEventOpen()) {
		m_tableModel->clearRows();
		m_tableFooterModel->reload();
		return;
	}

	QVariantMap qm;
	qm[QStringLiteral("stage_id")] = currentStageId();
	m_tableModel->setQueryParameters(qm);
	QElapsedTimer tm;
	tm.start();
	m_tableModel->reload();
	qfDebug() << m_tableModel->effectiveQuery();
	qfDebug() << "query execution time:" << tm.elapsed() << "msec";
	QTimer::singleShot(10, m_tableFooterView, &FooterView::syncSectionSizes);
}

void EventStatisticsWidget::onDbEventNotify(const QString &domain, const QVariant &payload)
{
	Q_UNUSED(payload)
	if(domain == QLatin1String(Event::EventPlugin::DBEVENT_CARD_PROCESSED_AND_ASSIGNED)
	   || domain == QLatin1String(Event::EventPlugin::DBEVENT_COMPETITOR_COUNTS_CHANGED)) {
		reloadDeferred();
	}
}

void EventStatisticsWidget::onVisibleChanged(bool is_visible)
{
	Q_UNUSED(is_visible)
	reload();
}

void EventStatisticsWidget::loadPersistentSettings()
{
	//qfWarning() << Q_FUNC_INFO;
}

void EventStatisticsWidget::savePersistentSettings()
{
	//qfWarning() << Q_FUNC_INFO;
}

int EventStatisticsWidget::currentStageId()
{
	int ret = getPlugin<EventPlugin>()->currentStageId();
	return ret;
}

void EventStatisticsWidget::initAutoRefreshTimer()
{
	EventStatisticsOptions::Options opts(options());
	int sec = opts.autoRefreshSec();
	if(sec > 0) {
		if(!m_autoRefreshTimer) {
			m_autoRefreshTimer = new QTimer(this);
			connect(m_autoRefreshTimer, &QTimer::timeout, this, &EventStatisticsWidget::reloadDeferred);
		}
		m_autoRefreshTimer->start(sec * 1000);
	}
	else {
		QF_SAFE_DELETE(m_autoRefreshTimer);
	}
}

void EventStatisticsWidget::onReloadClicked()
{
	qfLogFuncFrame();
	reload();
}

void EventStatisticsWidget::onPrintResultsSelectedClicked()
{
	QList<int> rows;
	for(int i : ui->tableView->selectedRowsIndexes())
		rows << ui->tableView->toTableModelRowNo(i);
	printResultsForRows(rows);
}

void EventStatisticsWidget::printResultsForRows(const QList<int> &rows)
{
	qfLogFuncFrame();// << rows;
	QStringList class_names;
	QList<int> classdefs_ids;
	QList<int> runners_finished;
	for(int i : rows) {
		qf::core::utils::TableRow row = m_tableModel->tableRow(i);
		class_names << row.value(QStringLiteral("classes.name")).toString();
		classdefs_ids << row.value(QStringLiteral("classdefs.id")).toInt();
		runners_finished << row.value(QStringLiteral("runnersFinished")).toInt();
	}
	bool report_printed = false;
	quickevent::gui::ReportOptionsDialog::Options opts;
	EventStatisticsOptions::Options stat_opts(options());
	bool with_dialog = stat_opts.isShowPrintDialog();
	if(with_dialog) {
		quickevent::gui::ReportOptionsDialog dlg(this);
		dlg.setPersistentSettingsId("resultsReportOptions");
		dlg.setClassNamesFilter(class_names);
		if(!dlg.exec())
			return;
		opts = dlg.options();
	}
	else {
		opts = quickevent::gui::ReportOptionsDialog::savedOptions("resultsReportOptions");
		opts.setUseClassFilter(true);
		opts.setClassFilterType((int)quickevent::gui::ReportOptionsDialog::FilterType::ClassName);
		opts.setClassFilter(class_names.join(','));
	}
	QVariant td = getPlugin<RunsPlugin>()->currentStageResultsTableData(quickevent::gui::ReportOptionsDialog::sqlWhereExpression(opts, getPlugin<EventPlugin>()->currentStageId()));
	QVariantMap props;
	props["isBreakAfterEachClass"] = (opts.breakType() != (int)quickevent::gui::ReportOptionsDialog::BreakType::None);
	props["isColumnBreak"] = (opts.breakType() == (int)quickevent::gui::ReportOptionsDialog::BreakType::Column);
	props["options"] = opts;
	report_printed = qf::gui::reports::ReportViewWidget::showReport(this
								, getPlugin<RunsPlugin>()->findReportFile("results_stage.qml")
								, td
								, tr("Results by classes")
								, "printCurrentStage"
								, props
								);
	if(report_printed) {
		clearNewResults(classdefs_ids, runners_finished);
		reload();
	}
}

void EventStatisticsWidget::onClearNewInSelectedRowsClicked()
{
	qfLogFuncFrame();
	QList<int> classdefs_ids;
	QList<int> runners_finished;
	QList<int> sel_rows = ui->tableView->selectedRowsIndexes();
	if(sel_rows.isEmpty())
		ui->tableView->selectAll();
	sel_rows = ui->tableView->selectedRowsIndexes();
	for(int i : sel_rows) {
		qf::core::utils::TableRow row = ui->tableView->tableRow(i);
		//class_names << row.value(QStringLiteral("classes.name")).toString();
		classdefs_ids << row.value(QStringLiteral("classdefs.id")).toInt();
		runners_finished << row.value(QStringLiteral("runnersFinished")).toInt();
	}
	clearNewResults(classdefs_ids, runners_finished);
	reload();
}

void EventStatisticsWidget::clearNewResults(const QList<int> &classdefs_ids, const QList<int> &runners_finished)
{
	qfLogFuncFrame();
	QString qs = "UPDATE classdefs SET resultsCount=:resultsCount, resultsPrintTS=:resultsPrintTS WHERE id=:id";
	qf::core::sql::Query q;
	q.prepare(qs, qf::core::Exception::Throw);
	for (int i = 0; i < classdefs_ids.count(); ++i) {
		qfDebug() << classdefs_ids[i] << runners_finished[i];
		q.bindValue(":resultsCount", runners_finished[i]);
		q.bindValue(":resultsPrintTS", QDateTime::currentDateTime());
		q.bindValue(":id", classdefs_ids[i]);
		q.exec(qf::core::Exception::Throw);
	}
}

QVariantMap EventStatisticsWidget::options()
{
	QSettings settings;
	QVariantMap m = settings.value(EventStatisticsOptions::staticPersistentSettingsPath()).toMap();
	return m;
}

void EventStatisticsWidget::onOptionsClicked()
{
	EventStatisticsOptions dlg(this);
	if(dlg.exec()) {
		initAutoRefreshTimer();
	}
}

void EventStatisticsWidget::onPrintResultsNewClicked()
{
	qfLogFuncFrame();
	reload();
	QList<int> sel_rows;
	EventStatisticsOptions::Options opts(options());
	int print_new_results_cnt = opts.autoPrintNewRunners();
	int print_new_results_sec = opts.autoPrintNewMin() * 60;
	for (int i = 0; i < m_tableModel->rowCount(); ++i) {
		int not_printed_cnt = m_tableModel->value(i, EventStatisticsModel::col_resultsNotPrinted).toInt();
		QVariant not_printed_ms_v = m_tableModel->value(i, EventStatisticsModel::col_resultsNotPrintedMs);
		int not_printed_sec = not_printed_ms_v.toInt() / 1000;
		int not_finished = m_tableModel->value(i, EventStatisticsModel::col_runnersNotFinished).toInt();
		qfDebug() << i
				  << m_tableModel->value(i, EventStatisticsModel::col_className).toString()
				  << "not printed cnt:" << not_printed_cnt
				  << "not printed sec:" << not_printed_sec << not_printed_ms_v
				  << "not_finished:" << not_finished;
		if((not_printed_ms_v.isNull() && not_printed_cnt > 0)
		   || (not_printed_sec > 0 && not_printed_sec >= print_new_results_sec && not_printed_cnt > 0)
		   || (not_printed_cnt >= print_new_results_cnt)
		   || (not_printed_cnt > 0 && not_finished == 0) // last competitor in class
		   ) {
			sel_rows << i;
			qfDebug() << i << "added";
		}
	}
	if(sel_rows.count()) {
		printResultsForRows(sel_rows);
	}
}


