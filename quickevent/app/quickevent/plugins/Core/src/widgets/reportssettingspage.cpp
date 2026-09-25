#include "reportssettingspage.h"
#include "ui_reportssettingspage.h"
// #include "../reportssettings.h"

#include <qf/core/log.h>
#include <qf/gui/framework/plugin.h>
#include <qf/gui/framework/reportfilecache.h>
#include <qf/gui/framework/mainwindow.h>
#include <qf/gui/style.h>

#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QFileDialog>
#include <QHeaderView>
#include <QSettings>
#include <QMessageBox>
#include <QTableWidget>

namespace Core {

namespace {
enum ReportFileColumn {
	FileNameColumn,
	FileSizeColumn,
	FileCreatedColumn,
	FileModifiedColumn,
	FileSameAsOriginalColumn,
	ReportFileColumnCount
};

bool isSameAsOriginal(const QString &file_path, const QString &relative_path)
{
	QFile local_file(file_path);
	QFile original_file(QStringLiteral(":/reports/") + relative_path);
	if (!local_file.open(QIODevice::ReadOnly) || !original_file.open(QIODevice::ReadOnly))
		return false;
	if (local_file.size() != original_file.size())
		return false;
	return local_file.readAll() == original_file.readAll();
}

}

ReportsSettingsPage::ReportsSettingsPage(QWidget *parent) :
	Super(parent),
	ui(new Ui::ReportsSettingsPage)
{
	m_caption = tr("Reports");
	ui->setupUi(this);

	ui->btResizeColumnsToFit->setIcon(qf::gui::Style::icon("zoom_fitwidth"));

	connect(ui->btResizeColumnsToFit, &QPushButton::clicked, this, &ReportsSettingsPage::resizeTableColumnsToFit);

	ui->tblReportFiles->setColumnCount(ReportFileColumnCount);
	ui->tblReportFiles->setHorizontalHeaderLabels({tr("Name"), tr("Size"), tr("Created"), tr("Modified"), tr("Same as original")});
	ui->tblReportFiles->setEditTriggers(QAbstractItemView::NoEditTriggers);
	ui->tblReportFiles->setSelectionBehavior(QAbstractItemView::SelectRows);
	ui->tblReportFiles->setSelectionMode(QAbstractItemView::SingleSelection);

	ui->tblReportFiles->setSortingEnabled(true);
	ui->tblReportFiles->horizontalHeader()->setStretchLastSection(true);

	connect(ui->btClearLocalChanges, &QPushButton::clicked, this, [this]() {
		qf::gui::framework::Plugin::reportFileCache()->clearLocalChanges();
		load();
	});
}

ReportsSettingsPage::~ReportsSettingsPage()
{
	delete ui;
}

void ReportsSettingsPage::resizeTableColumnsToFit()
{
	ui->tblReportFiles->resizeColumnsToContents();
}

void ReportsSettingsPage::load()
{
	const auto dir = qf::gui::framework::Plugin::reportFileCache()->effectiveReportsDir();
	ui->edReportsDirectory->setText(dir);

	ui->tblReportFiles->setSortingEnabled(false);
	ui->tblReportFiles->setRowCount(0);
	const QDir reports_dir(dir);
	QDirIterator iterator(dir, QDir::Files | QDir::Hidden | QDir::System, QDirIterator::Subdirectories);
	while (iterator.hasNext()) {
		const QFileInfo file_info(iterator.next());
		const QString relative_path = reports_dir.relativeFilePath(file_info.filePath());
		const int row = ui->tblReportFiles->rowCount();
		ui->tblReportFiles->insertRow(row);
		ui->tblReportFiles->setItem(row, FileNameColumn, new QTableWidgetItem(relative_path));
		ui->tblReportFiles->setItem(row, FileSizeColumn, new QTableWidgetItem(QString::number(file_info.size())));
		ui->tblReportFiles->setItem(row, FileCreatedColumn, new QTableWidgetItem(file_info.birthTime().toString(Qt::ISODate)));
		ui->tblReportFiles->setItem(row, FileModifiedColumn, new QTableWidgetItem(file_info.lastModified().toString(Qt::ISODate)));
		auto *same_as_original_item = new QTableWidgetItem;
		same_as_original_item->setCheckState(isSameAsOriginal(file_info.filePath(), relative_path) ? Qt::Checked : Qt::Unchecked);
		ui->tblReportFiles->setItem(row, FileSameAsOriginalColumn, same_as_original_item);
	}
	ui->tblReportFiles->setSortingEnabled(true);
}

void ReportsSettingsPage::save()
{
	//ReportsSettings settings;
	// nothing to save for now
}

}
