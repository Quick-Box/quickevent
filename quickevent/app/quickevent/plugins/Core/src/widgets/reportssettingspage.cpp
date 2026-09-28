#include "reportssettingspage.h"
#include "ui_reportssettingspage.h"
// #include "../reportssettings.h"

#include <qf/core/log.h>
#include <qf/core/sql/query.h>
#include <qf/gui/framework/plugin.h>
#include <qf/gui/framework/reportfilecache.h>
#include <qf/gui/framework/mainwindow.h>
#include <qf/gui/style.h>

#include <QCryptographicHash>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QFileDialog>
#include <QHeaderView>
#include <QSettings>
#include <QMessageBox>
#include <QPushButton>
#include <QTableWidget>

namespace Core {

namespace {
enum ReportFileColumn {
	FileNameColumn,
	FileSizeColumn,
	FileCreatedColumn,
	FileModifiedColumn,
	FileSameAsOriginalColumn,
	SaveToDbColumn,
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

bool saveReportToDb(const QString &file_path, const QString &relative_path, QString *error_text)
{
	QFile file(file_path);
	if (!file.open(QIODevice::ReadOnly)) {
		if (error_text)
			*error_text = file.errorString();
		return false;
	}
	const QByteArray data = file.readAll();
	const qint64 size = file.size();
	const QString hash = QString::fromLatin1(QCryptographicHash::hash(data, QCryptographicHash::Sha1).toHex());

	qf::core::sql::Query update_query;
	update_query.prepare(QStringLiteral("UPDATE reports SET data=:data, hash=:hash, size=:size WHERE path=:path"));
	update_query.bindValue(QStringLiteral(":path"), relative_path);
	update_query.bindValue(QStringLiteral(":data"), data);
	update_query.bindValue(QStringLiteral(":hash"), hash);
	update_query.bindValue(QStringLiteral(":size"), size);
	if (!update_query.exec()) {
		if (error_text)
			*error_text = update_query.lastErrorText();
		return false;
	}
	if (update_query.numRowsAffected() > 0)
		return true;

	qf::core::sql::Query insert_query;
	insert_query.prepare(QStringLiteral("INSERT INTO reports(path, data, hash, size) VALUES(:path, :data, :hash, :size)"));
	insert_query.bindValue(QStringLiteral(":path"), relative_path);
	insert_query.bindValue(QStringLiteral(":data"), data);
	insert_query.bindValue(QStringLiteral(":hash"), hash);
	insert_query.bindValue(QStringLiteral(":size"), size);
	if (!insert_query.exec()) {
		if (error_text)
			*error_text = insert_query.lastErrorText();
		return false;
	}
	return true;
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
	ui->tblReportFiles->setHorizontalHeaderLabels({tr("Name"), tr("Size"), tr("Created"), tr("Modified"), tr("Same as original"), tr("Save to DB")});
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

		auto *save_button = new QPushButton(tr("Save"), ui->tblReportFiles);
		connect(save_button, &QPushButton::clicked, this, [this, file_path = file_info.filePath(), relative_path]() {
			QString error_text;
			if (!saveReportToDb(file_path, relative_path, &error_text)) {
				QMessageBox::warning(this, tr("Save report"), tr("Failed to save report '%1' to the database:\n%2").arg(relative_path, error_text));
				return;
			}
			QMessageBox::information(this, tr("Save report"), tr("Report '%1' was saved to the database.").arg(relative_path));
		});
		ui->tblReportFiles->setCellWidget(row, SaveToDbColumn, save_button);
	}
	ui->tblReportFiles->setSortingEnabled(true);
}

void ReportsSettingsPage::save()
{
	//ReportsSettings settings;
	// nothing to save for now
}

}
