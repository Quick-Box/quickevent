#include "runstablewidget.h"
#include "ui_runstablewidget.h"
#include "runstablemodel.h"
#include "runstableitemdelegate.h"
#include "runsplugin.h"
#include "runswidget.h"
#include "runflagsdialog.h"
#include "cardflagsdialog.h"

#include <quickevent/core/si/siid.h>
#include <quickevent/core/og/timems.h>

#include <qf/gui/dialogs/getiteminputdialog.h>
#include <qf/gui/dialogs/messagebox.h>
#include <qf/gui/framework/mainwindow.h>
#include <qf/gui/framework/plugin.h>

#include <qf/core/sql/query.h>
#include <qf/core/sql/transaction.h>
#include <qf/core/log.h>
#include <qf/core/assert.h>
#include <plugins/Event/src/eventplugin.h>
#include <plugins/CardReader/src/cardreaderplugin.h>
#include <plugins/Receipts/src/receiptsplugin.h>

#include <QSortFilterProxyModel>
#include <QMenu>
#include <QTimer>
#include <QInputDialog>

namespace qfc = qf::core;
namespace qfs = qf::core::sql;
namespace qfw = qf::gui;
using qf::gui::framework::getPlugin;
using Event::EventPlugin;
using Runs::RunsPlugin;
using Receipts::ReceiptsPlugin;

RunsTableWidget::RunsTableWidget(QWidget *parent) :
	Super(parent),
	ui(new Ui::RunsTableWidget)
{
	ui->setupUi(this);

	ui->tblRunsToolBar->setTableView(ui->tblRuns);

	ui->tblRuns->setShowExceptionDialog(false);
	connect(ui->tblRuns, &qf::gui::TableView::sqlException, this, &RunsTableWidget::onTableViewSqlException, Qt::QueuedConnection);
	// ui->tblRuns->setEditRowsMenuSectionEnabled(false);
	ui->tblRuns->setCloneRowEnabled(false);
	ui->tblRuns->setDirtyRowsMenuSectionEnabled(false);
	ui->tblRuns->setPersistentSettingsId("tblRuns");
	ui->tblRuns->setRowEditorMode(qfw::TableView::EditRowsMixed);
	ui->tblRuns->setInlineEditSaveStrategy(qfw::TableView::OnEditedValueCommit);
	m_runsTableItemDelegate = new RunsTableItemDelegate(ui->tblRuns);
	ui->tblRuns->setItemDelegate(m_runsTableItemDelegate);

	//ui->tblRuns->setSelectionMode(QTableView::SingleSelection);
	ui->tblRuns->viewport()->setAcceptDrops(true);
	ui->tblRuns->setDropIndicatorShown(true);
	ui->tblRuns->setDragDropMode(QAbstractItemView::InternalMove);
	ui->tblRuns->setDragEnabled(false);
	ui->tblRuns->setContextMenuPolicy(Qt::CustomContextMenu);
	connect(ui->tblRuns, &qfw::TableView::customContextMenuRequested, this, &RunsTableWidget::onCustomContextMenuRequest);

	m_runsModel = new RunsTableModel(this);
	connect(m_runsModel, &RunsTableModel::badDataInput, this, &RunsTableWidget::onBadTableDataInput, Qt::QueuedConnection);
	connect(m_runsModel, &RunsTableModel::runnerSiIdEdited, getPlugin<RunsPlugin>(), &Runs::RunsPlugin::clearRunnersTableCache);
	ui->tblRuns->setTableModel(m_runsModel);

	// this ensures that table is sorted every time when start time is edited
	ui->tblRuns->sortFilterProxyModel()->setDynamicSortFilter(true);

	connect(m_runsModel, &RunsTableModel::startTimesSwitched, this, [this](int id1, int id2, const QString &err_msg)
	{
		Q_UNUSED(id1)
		Q_UNUSED(id2)
		if(!err_msg.isEmpty()) {
			qf::gui::dialogs::MessageBox::showError(this, err_msg);
		}
		//ui->tblRuns->reload(true);
		m_runsModel->reload();
	});

	connect(ui->tblRuns->horizontalHeader(), &QHeaderView::sortIndicatorChanged, this, &RunsTableWidget::updateStartTimeHighlight);

	connect(ui->tblRuns, &qfw::TableView::editRowInExternalEditor, this, [this](const QVariant &, int mode) {
		auto *tv = ui->tblRuns;
		auto ix = tv->currentIndex();
		auto col = ix.column();
		if(col == RunsTableModel::col_runFlags || col == RunsTableModel::col_cardFlags) {
			return;
		}
		if (mode == qf::gui::model::DataDocument::ModeInsert) {
			emit editCompetitorRequest(0, mode);
		}
		else {
			auto competitor_id = tv->selectedRow().value("competitorId");
			emit editCompetitorRequest(competitor_id.toInt(), mode);
		}
	});
	connect(ui->tblRuns, &qfw::TableView::editCellRequest, this, [this](QModelIndex index) {
		auto col = index.column();
		if(col == RunsTableModel::col_runFlags) {
			Runs::RunFlagsDialog dlg(this);
			dlg.load(m_runsModel, ui->tblRuns->toTableModelRowNo(ui->tblRuns->currentIndex().row()));
			if(dlg.exec()) {
				dlg.save();
			}
		}
		else if(col == RunsTableModel::col_cardFlags) {
			Runs::CardFlagsDialog dlg(this);
			dlg.load(m_runsModel, ui->tblRuns->toTableModelRowNo(ui->tblRuns->currentIndex().row()));
			if(dlg.exec()) {
				dlg.save();
			}
		}
	}, Qt::QueuedConnection);
}

RunsTableWidget::~RunsTableWidget()
{
	delete ui;
}

void RunsTableWidget::updateStartTimeHighlight() const
{
	auto logical_index = ui->tblRuns->horizontalHeader()->sortIndicatorSection();
	auto order = ui->tblRuns->horizontalHeader()->sortIndicatorOrder();
	auto cd = m_runsModel->columnDefinition(logical_index);
	bool is_sort_start_time_asc = (cd.matchesSqlId(QStringLiteral("runs.startTimeMs"))
								   && order == Qt::AscendingOrder
								   && ui->tblRuns->filterString().isEmpty());
	m_runsTableItemDelegate->setStartTimeHighlightVisible(is_sort_start_time_asc);
	ui->tblRuns->setDragEnabled(is_sort_start_time_asc);
}

void RunsTableWidget::clear()
{
	m_runsModel->clearRows();
}

void RunsTableWidget::reload(int stage_id, int class_id, bool show_offrace, const QString &sort_column, int select_competitor_id)
{
	qfLogFuncFrame() << "class id:" << class_id;
	{
		int class_start_time_min = 0;
		int class_start_interval_min = 0;
		if(class_id > 0) {
			qf::core::sql::Query q;
			q.exec("SELECT startTimeMin, startIntervalMin FROM classdefs WHERE classId=" QF_IARG(class_id) " AND stageId=" QF_IARG(stage_id), qf::core::Exception::Throw);
			if(q.next()) {
				class_start_time_min = q.value(0).toInt();
				class_start_interval_min = q.value(1).toInt();
			}
		}
		ui->lblClassStart->setText(class_start_time_min >= 0? QString::number(class_start_time_min): "---");
		ui->lblClassInterval->setText(class_start_interval_min >= 0? QString::number(class_start_interval_min): "---");
	}
	bool is_relays = getPlugin<EventPlugin>()->eventConfig()->isRelays();
	auto qb = getPlugin<RunsPlugin>()->runsQuery(stage_id, class_id, show_offrace);
	qfDebug() << qb.toString();
	m_runsTableItemDelegate->setHighlightedClassId(class_id, stage_id);
	m_runsModel->setQueryBuilder(qb, false);
	m_runsModel->reload();
	updateStartTimeHighlight();

	QHeaderView *hh = ui->tblRuns->horizontalHeader();
	hh->setSectionHidden(RunsTableModel::col_runs_isRunning, !show_offrace);
	if(show_offrace) {
		// section might be hidden from previous persistent settings
		if(hh->sectionSize(RunsTableModel::col_runs_isRunning) < 5)
			hh->resizeSection(RunsTableModel::col_runs_isRunning, 50);
	}
	hh->setSectionHidden(RunsTableModel::col_relays_name, !is_relays);
	hh->setSectionHidden(RunsTableModel::col_runs_leg, !is_relays);
	if(is_relays) {
		if(hh->sectionSize(RunsTableModel::col_relays_name) < 5)
			hh->resizeSection(RunsTableModel::col_relays_name, 50);
		if(hh->sectionSize(RunsTableModel::col_runs_leg) < 5)
			hh->resizeSection(RunsTableModel::col_runs_leg, 50);
	}
	hh->reset(); // revealed columns are not sometimes visible without reset(), atleast in 5.5.1

	if(!sort_column.isEmpty()) {
		int sort_col_ix = m_runsModel->columnIndex(sort_column);
		if(sort_col_ix >= 0) {
			QHeaderView *hdrv = hh;
			hdrv->setSortIndicator(sort_col_ix, Qt::AscendingOrder);
			if(select_competitor_id > 0) {
				for (int i = 0; i < m_runsModel->rowCount(); ++i) {
					int competitor_id = m_runsModel->table().row(i).value(QStringLiteral("competitorId")).toInt();
					if(competitor_id == select_competitor_id) {
						QModelIndex ix = m_runsModel->index(i, sort_col_ix);
						ix = ui->tblRuns->sortFilterProxyModel()->mapFromSource(ix);
						ui->tblRuns->setCurrentIndex(ix);
						QTimer::singleShot(0, this, [this, ix]() {
							this->ui->tblRuns->scrollTo(ix);
						});
					}
				}
			}
		}
	}
}

void RunsTableWidget::reload()
{
	runsModel()->reload();
	m_runsTableItemDelegate->reloadHighlightedClassId();
	updateStartTimeHighlight();
}

qf::gui::TableView *RunsTableWidget::tableView()
{
	return ui->tblRuns;
}

void RunsTableWidget::onCustomContextMenuRequest(const QPoint &pos)
{
	qfLogFuncFrame();
	QAction a_show_receipt(tr("Show receipt"), nullptr);
	QAction a_load_card(tr("Load times from card in selected rows"), nullptr);
	QAction a_print_card(tr("Print receipt"), nullptr);
	QAction a_sep1(nullptr); a_sep1.setSeparator(true);
	QAction a_shift_start_times(tr("Shift start times in selected rows"), nullptr);
	QAction a_clear_start_times(tr("Clear start times in selected rows"), nullptr);
	QAction a_change_class(tr("Set class in selected rows"), nullptr);
	QList<QAction*> lst;
	lst << &a_show_receipt << &a_load_card << &a_print_card
		<< &a_sep1
		<< &a_shift_start_times
		<< &a_clear_start_times
		<< &a_change_class;
	QAction *a = QMenu::exec(lst, ui->tblRuns->viewport()->mapToGlobal(pos));
	if(a == &a_load_card) {
		qf::gui::framework::MainWindow *fwk = qf::gui::framework::MainWindow::frameWork();
		int curr_ix = 0;
		QList<int> sel_ixs = ui->tblRuns->selectedRowsIndexes();
		QList<int> runid_to_reload;
		for(int ix : sel_ixs) {
			qf::core::utils::TableRow row = ui->tblRuns->tableRow(ix);
			int run_id = row.value(QStringLiteral("runs.id")).toInt();
			fwk->showProgress(tr("Reloading times for %1").arg(row.value(QStringLiteral("competitorName")).toString()), ++curr_ix, sel_ixs.count());
			bool ok = getPlugin<RunsPlugin>()->reloadTimesFromCard(run_id);
			//qfInfo() << ix << run_id << ok << row.value(QStringLiteral("competitorName")).toString();
			if(ok)
				runid_to_reload << run_id;
		}
		auto *m = ui->tblRuns->tableModel();
		for(int runid : runid_to_reload) {
			// we cannot reload row-ix, because proxy-model might be sorted according to reloaded value
			for(int i = 0; i < m->rowCount(); ++i) {
				int id = m->value(i, QStringLiteral("runs.id")).toInt();
				if(id == runid) {
					m->reloadRow(i);
				}
			}
		}
		fwk->hideProgress();
	}
	else if(a == &a_show_receipt) {
		int run_id = ui->tblRuns->tableRow().value(QStringLiteral("runs.id")).toInt();
		int card_id = getPlugin<RunsPlugin>()->cardForRun(run_id);
		if(card_id > 0) {
			getPlugin<ReceiptsPlugin>()->previewReceipt(card_id);
		}
	}
	else if(a == &a_print_card) {
		int run_id = ui->tblRuns->tableRow().value(QStringLiteral("runs.id")).toInt();
		int card_id = getPlugin<RunsPlugin>()->cardForRun(run_id);
		if(card_id > 0) {
			getPlugin<ReceiptsPlugin>()->printReceipt(card_id);
		}
	}
	else if(a == &a_shift_start_times) {
		try {
			int offset_msec = 0;
			int interval = ui->lblClassInterval->text().toInt();
			if(interval == 0)
				interval = 1;
			bool ok;
			offset_msec = QInputDialog::getInt(this, tr("Get number"), tr("Start times offset [min]:"), 0, -1000, 1000, interval, &ok);
			if(ok) {
				offset_msec *= 60 * 1000;

				qfs::Transaction transaction;
				qfs::Query q(transaction.connection());
				q.prepare("UPDATE runs SET startTimeMs = COALESCE(startTimeMs, 0) + :offset WHERE id=:id", qf::core::Exception::Throw);
				QList<int> rows = ui->tblRuns->selectedRowsIndexes();
				for(int ix : rows) {
					qf::core::utils::TableRow row = ui->tblRuns->tableRow(ix);
					int id = row.value(ui->tblRuns->tableModel()->idColumnName()).toInt();
					q.bindValue(QStringLiteral(":offset"), offset_msec);
					q.bindValue(QStringLiteral(":id"), id);
					//qfInfo() << id << "->" << offset_msec;
					q.exec(qf::core::Exception::Throw);
				}
				transaction.commit();
				runsModel()->reload();
			}
		}
		catch (const qf::core::Exception &e) {
			qf::gui::dialogs::MessageBox::showException(this, e);
		}
	}
	else if(a == &a_clear_start_times) {
		try {
			qfs::Transaction transaction;
			qfs::Query q(transaction.connection());
			q.prepare("UPDATE runs SET startTimeMs = NULL WHERE id=:id", qf::core::Exception::Throw);
			QList<int> rows = ui->tblRuns->selectedRowsIndexes();
			for(int ix : rows) {
				qf::core::utils::TableRow row = ui->tblRuns->tableRow(ix);
				int id = row.value(ui->tblRuns->tableModel()->idColumnName()).toInt();
				q.bindValue(QStringLiteral(":id"), id);
				//qfInfo() << id << "->" << offset_msec;
				q.exec(qf::core::Exception::Throw);
			}
			transaction.commit();
			runsModel()->reload();
		}
		catch (const qf::core::Exception &e) {
			qf::gui::dialogs::MessageBox::showException(this, e);
		}
	}
	else if(a == &a_change_class) {
		qfw::dialogs::GetItemInputDialog dlg(this);
		QComboBox *box = dlg.comboBox();
		qfs::Query q;
		q.exec("SELECT id, name FROM classes ORDER BY name");
		while (q.next()) {
			box->addItem(q.value(1).toString(), q.value(0));
		}
		dlg.setWindowTitle(tr("Dialog"));
		dlg.setLabelText(tr("Select class"));
		dlg.setCurrentItemIndex(-1);
		if(dlg.exec()) {
			int class_id = dlg.currentData().toInt();
			if(class_id > 0) {
				qfs::Transaction transaction;
				try {
					QList<int> rows = ui->tblRuns->selectedRowsIndexes();
					for(int i : rows) {
						qf::core::utils::TableRow row = ui->tblRuns->tableRowRef(i);
						int competitor_id = row.value("competitors.id").toInt();
						q.exec(QString("UPDATE competitors SET classId=%1 WHERE id=%2").arg(class_id).arg(competitor_id), qfc::Exception::Throw);
					}
					transaction.commit();
				}
				catch (std::exception &e) {
					qfError() << e.what();
				}
			}
			ui->tblRuns->reload(true);
		}
	}
}

void RunsTableWidget::onTableViewSqlException(const QString &what, const QString &where, const QString &stack_trace)
{
	if(what.contains(QLatin1String("runs.stageId")) && what.contains(QLatin1String("runs.siId"))) {
		// "UNIQUE constraint failed: runs.stageId, runs.siId Unable to fetch row"
		// duplicate SI insertion attempt
		qf::gui::dialogs::MessageBox::showError(this, tr("Duplicate SI inserted."));
		return;
	}
	qf::gui::dialogs::MessageBox::showException(this, what, where, stack_trace);
}

void RunsTableWidget::onBadTableDataInput(const QString &message)
{
	qf::gui::dialogs::MessageBox::showError(this, message);
}


