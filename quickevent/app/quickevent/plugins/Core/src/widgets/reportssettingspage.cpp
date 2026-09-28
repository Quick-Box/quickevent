#include "reportssettingspage.h"
#include "reportstablemodel.h"
#include "ui_reportssettingspage.h"
// #include "../reportssettings.h"

#include <qf/core/log.h>
#include <qf/gui/framework/plugin.h>
#include <qf/gui/framework/reportfilecache.h>
#include <qf/gui/framework/mainwindow.h>
#include <qf/gui/style.h>

#include <QFileDialog>
#include <QHeaderView>
#include <QSettings>
#include <QMessageBox>
#include <QPushButton>
#include <QSortFilterProxyModel>
#include <QMenu>

namespace Core {

ReportsSettingsPage::ReportsSettingsPage(QWidget *parent) :
	Super(parent),
	ui(new Ui::ReportsSettingsPage),
	m_reportModel(new ReportsTableModel(this)),
	m_reportProxyModel(new ::QSortFilterProxyModel(this))
{
	m_caption = tr("Reports");
	ui->setupUi(this);

	ui->btResizeColumnsToFit->setIcon(qf::gui::Style::icon("zoom_fitwidth"));

	connect(ui->btResizeColumnsToFit, &QPushButton::clicked, this, &ReportsSettingsPage::resizeTableColumnsToFit);

	m_reportProxyModel->setSourceModel(m_reportModel);
	m_reportProxyModel->setSortRole(Qt::DisplayRole);
	ui->tblReportFiles->setModel(m_reportProxyModel);
	ui->tblReportFiles->setEditTriggers(QAbstractItemView::NoEditTriggers);
	ui->tblReportFiles->setSelectionMode(QAbstractItemView::SingleSelection);
	ui->tblReportFiles->setContextMenuPolicy(Qt::CustomContextMenu);
	connect(ui->tblReportFiles, &QTableView::customContextMenuRequested, this, &ReportsSettingsPage::showReportContextMenu);

	ui->tblReportFiles->setSortingEnabled(true);
	ui->tblReportFiles->horizontalHeader()->setStretchLastSection(true);

	connect(ui->btClearLocalChanges, &QPushButton::clicked, this, [this]() {
		qf::gui::framework::Plugin::reportFileCache()->clearLocalChanges();
		loadModel();
	});
}

ReportsSettingsPage::~ReportsSettingsPage()
{
	delete ui;
}

void ReportsSettingsPage::showReportContextMenu(const QPoint &position)
{
	const QModelIndex proxy_index = ui->tblReportFiles->indexAt(position);
	if (!proxy_index.isValid())
		return;
	ui->tblReportFiles->setCurrentIndex(proxy_index);
	const QModelIndex source_index = m_reportProxyModel->mapToSource(proxy_index);
	const auto report_path = m_reportModel->reportAt(source_index.row()).relativePath;

	const auto &report = m_reportModel->reportAt(source_index.row());
	QMenu menu(this);
	auto *save_to_db_action = menu.addAction(tr("Save to DB"));
	save_to_db_action->setEnabled(report.databaseHash != report.localHash);
	auto *clear_db_action = menu.addAction(tr("Clear DB entry"));
	clear_db_action->setEnabled(!report.databaseHash.isEmpty());
	auto *restore_db_action = menu.addAction(tr("Restore from DB"));
	restore_db_action->setEnabled(!report.databaseHash.isEmpty() && report.databaseHash != report.localHash);
	auto *restore_resources_action = menu.addAction(tr("Restore from resources"));
	restore_resources_action->setEnabled(report.resourcesHash != report.localHash);

	connect(save_to_db_action, &QAction::triggered, this, [this, source_index, report_path]() {
		QString error_text;
		if (!m_reportModel->saveReportToDb(source_index, &error_text)) {
			QMessageBox::warning(this, tr("Save report"), tr("Failed to save report '%1' to the database:\\n%2").arg(report_path, error_text));
			return;
		}
	});
	connect(clear_db_action, &QAction::triggered, this, [this, source_index, report_path]() {
		QString error_text;
		if (!m_reportModel->clearReportFromDb(source_index, &error_text)) {
			QMessageBox::warning(this, tr("Clear report database entry"), tr("Failed to clear database entry for report '%1':\\n%2").arg(report_path, error_text));
			return;
		}
	});
	connect(restore_db_action, &QAction::triggered, this, [this, source_index, report_path]() {
		QString error_text;
		if (!m_reportModel->restoreReportFromDb(source_index, &error_text)) {
			QMessageBox::warning(this, tr("Restore report"), tr("Failed to restore report '%1' from the database:\\n%2").arg(report_path, error_text));
			return;
		}
	});
	connect(restore_resources_action, &QAction::triggered, this, [this, source_index, report_path]() {
		QString error_text;
		if (!m_reportModel->restoreReportFromResources(source_index, &error_text)) {
			QMessageBox::warning(this, tr("Restore report"), tr("Failed to restore report '%1' from resources:\\n%2").arg(report_path, error_text));
			return;
		}
		loadModel();
	});
	menu.exec(ui->tblReportFiles->viewport()->mapToGlobal(position));
}

void ReportsSettingsPage::resizeTableColumnsToFit()
{
	ui->tblReportFiles->resizeColumnsToContents();
}

void ReportsSettingsPage::load()
{
	const auto dir = qf::gui::framework::Plugin::reportFileCache()->localReportsDir();
	ui->edReportsDirectory->setText(dir);
	loadModel();
	ui->tblReportFiles->horizontalHeader()->resizeSections(QHeaderView::ResizeToContents);
}

void ReportsSettingsPage::save()
{
	//ReportsSettings settings;
	// nothing to save for now
}

void ReportsSettingsPage::loadModel()
{
	ui->tblReportFiles->setSortingEnabled(false);
	m_reportModel->load();
	ui->tblReportFiles->setSortingEnabled(true);
}

}
