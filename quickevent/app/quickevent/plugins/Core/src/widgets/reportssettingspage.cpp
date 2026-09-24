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
}

ReportsSettingsPage::~ReportsSettingsPage()
{
	delete ui;
}

void ReportsSettingsPage::load()
{
	auto dir = qf::gui::framework::Plugin::reportFileCache()->effectiveReportsDir();
	ui->edReportsDirectory->setText(dir);
}

void ReportsSettingsPage::save()
{
	//ReportsSettings settings;
	// nothing to save for now
}

}

