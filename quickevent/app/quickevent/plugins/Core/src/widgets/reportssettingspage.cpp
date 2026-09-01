#include "reportssettingspage.h"
#include "ui_reportssettingspage.h"
#include "../reportssettings.h"

#include <qf/core/log.h>
#include <qf/gui/framework/plugin.h>
#include <qf/gui/framework/reportfilecache.h>
#include <qf/gui/framework/mainwindow.h>

#include <QDirIterator>
#include <QFileDialog>
#include <QSettings>
#include <QMessageBox>

namespace Core {

ReportsSettingsPage::ReportsSettingsPage(QWidget *parent) :
	Super(parent),
	ui(new Ui::ReportsSettingsPage)
{
	m_caption = tr("Reports");
	ui->setupUi(this);

	connect(ui->btSelectCustomReportsDirectory, &QAbstractButton::clicked, this, &ReportsSettingsPage::onSelectCustomReportsDirectoryClicked);

	ui->edReportsDirectory->setPlaceholderText(qf::gui::framework::Plugin::reportFileCache()->defaultReportsDir());
	ui->lblHelp->setText(ui->lblHelp->text().arg(qf::gui::framework::Plugin::reportFileCache()->defaultReportsDir()));
	
	connect(ui->btSetDefaultReportsDir, &QAbstractButton::clicked, this, [this]() {
		setReportsDirectory({});
	});
}

ReportsSettingsPage::~ReportsSettingsPage()
{
	delete ui;
}

void ReportsSettingsPage::setReportsDirectory(const QString dir)
{
	auto default_reports_dir = qf::gui::framework::Plugin::reportFileCache()->defaultReportsDir();
	if (dir == default_reports_dir) {
		ui->edReportsDirectory->setText({});
	}
	else {
		ui->edReportsDirectory->setText(dir);
	}
}

void ReportsSettingsPage::onSelectCustomReportsDirectoryClicked()
{
	auto old_dir = ui->edReportsDirectory->text();
	auto new_dir = QFileDialog::getExistingDirectory(this, tr("Open Directory"),
													 old_dir,
													 QFileDialog::ShowDirsOnly);
	if(new_dir.isEmpty())
		return;
	setReportsDirectory(new_dir);
}

static const auto KEY_reportsDirectory = QStringLiteral("reportsDirectory");
//static const auto KEY_exportReportDefinitionsDir = QStringLiteral("exportReportDefinitionsDir");

void ReportsSettingsPage::load()
{
	auto dir = reportsDirectoryFromSettings();
	setReportsDirectory(dir);
}

void ReportsSettingsPage::save()
{
	ReportsSettings settings;
	auto dir = ui->edReportsDirectory->text().trimmed();
	settings.setReportsDirectory(dir);
	qf::gui::framework::Plugin::reportFileCache()->setReportsDir(dir);
}

QString ReportsSettingsPage::reportsDirectoryFromSettings() const
{
	ReportsSettings settings;
	auto dir = settings.reportsDirectory();
	if(dir.isEmpty())
		dir = qf::gui::framework::Plugin::reportFileCache()->defaultReportsDir();
	return dir;
}

}

