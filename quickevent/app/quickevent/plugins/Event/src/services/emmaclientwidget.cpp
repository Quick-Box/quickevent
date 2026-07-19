#include "emmaclientwidget.h"
#include "ui_emmaclientwidget.h"
#include "emmaclient.h"
#include "../eventplugin.h"
#include <qf/gui/framework/mainwindow.h>

#include <qf/gui/dialogs/messagebox.h>

#include <qf/core/assert.h>

#include <QFileDialog>
using qf::gui::framework::getPlugin;


namespace Event::services {

EmmaClientWidget::EmmaClientWidget(QWidget *parent)
	: Super(parent)
	, ui(new Ui::EmmaClientWidget)
{
	setPersistentSettingsId("EmmaClientWidget");
	ui->setupUi(this);

	EmmaClient *svc = service();
	if(svc) {
		EmmaClientSettings ss = svc->settings();
		ui->edExportDir->setText(ss.exportDir());
		ui->edFileNameBase->setText(ss.fileNameBase());
		ui->edExportInterval->setValue(ss.exportIntervalSec());
		ui->chExportStartListTxt->setCheckState((ss.exportStartTypeTxt()) ? Qt::Checked : Qt::Unchecked);
		ui->chExportFinishTxt->setCheckState((ss.exportFinishTypeTxt()) ? Qt::Checked : Qt::Unchecked);
		ui->chExportStartListXml30->setCheckState((ss.exportStartListTypeXml3()) ? Qt::Checked : Qt::Unchecked);
		ui->chExportResultsXml30->setCheckState((ss.exportResultTypeXml3()) ? Qt::Checked : Qt::Unchecked);
		if (ui->edFileNameBase->text().isEmpty())
			ui->edFileNameBase->setText(getPlugin<EventPlugin>()->eventDbName());
		if (ss.startExportType() == EmmaClientSettings::StartExportType::CSV)
			ui->rbRacomStartCsv->setChecked(true);
	}

	connect(ui->btChooseExportDir, &QPushButton::clicked, this, &EmmaClientWidget::onBtChooseExportDirClicked);
	connect(ui->btExportSplitsTxt, &QPushButton::clicked, this, &EmmaClientWidget::onBtExportSplitsTxtClicked);
	connect(ui->btExportFinishTxt, &QPushButton::clicked, this, &EmmaClientWidget::onBtExportFinishTxtClicked);
	connect(ui->btExportStartListTxt, &QPushButton::clicked, this, &EmmaClientWidget::onBtExportStartListTxtClicked);
	connect(ui->btExportResultsXml30, &QPushButton::clicked, this, &EmmaClientWidget::onBtExportResultsXml30Clicked);
	connect(ui->btExportStartListXml30, &QPushButton::clicked, this, &EmmaClientWidget::onBtExportStartListXml30Clicked);
}

EmmaClientWidget::~EmmaClientWidget()
{
	delete ui;
}

void EmmaClientWidget::onBtChooseExportDirClicked()
{
	EmmaClient *svc = service();
	if(svc) {
		EmmaClientSettings ss = svc->settings();
		QString dir = QFileDialog::getExistingDirectory(this, tr("Open Directory"), ss.exportDir(), QFileDialog::ShowDirsOnly);
		if(!dir.isEmpty())
			ui->edExportDir->setText(dir);
	}
}

void EmmaClientWidget::onBtExportSplitsTxtClicked()
{
	EmmaClient *svc = service();
	if(svc) {
		saveSettings();
		svc->exportRadioCodesRacomTxt();
	}
}

bool EmmaClientWidget::acceptDialogDone(int result)
{
	if(result == QDialog::Accepted) {
		if(!saveSettings()) {
			qf::gui::dialogs::MessageBox::showError(this, tr("Cannot create directory '%1'.").arg(ui->edExportDir->text().trimmed()));
			return false;
		}
	}
	return true;
}

EmmaClient *EmmaClientWidget::service()
{
	auto *svc = qobject_cast<EmmaClient*>(Service::serviceByName(EmmaClient::serviceName()));
	QF_ASSERT(svc, EmmaClient::serviceName() + " doesn't exist", return nullptr);
	return svc;
}

bool EmmaClientWidget::saveSettings()
{
	EmmaClient *svc = service();
	if(svc) {
		EmmaClientSettings ss = svc->settings();
		QString dir = ui->edExportDir->text().trimmed();
		ss.setExportDir(dir);
		ss.setExportIntervalSec(ui->edExportInterval->value());
		ss.setFileNameBase(ui->edFileNameBase->text().trimmed());
		ss.setExportStartTypeTxt(ui->chExportStartListTxt->isChecked());
		ss.setExportFinishTypeTxt(ui->chExportFinishTxt->isChecked());
		ss.setExportStartListTypeXml3(ui->chExportStartListXml30->isChecked());;
		ss.setExportResultTypeXml3(ui->chExportResultsXml30->isChecked());;
		EmmaClientSettings::StartExportType type = EmmaClientSettings::StartExportType::TXT;
		if (ui->rbRacomStartCsv->isChecked())
			type = EmmaClientSettings::StartExportType::CSV;
		ss.setStartExportType(type);
		if (ss.fileNameBase().isEmpty())
			ss.setFileNameBase(getPlugin<EventPlugin>()->eventDbName());

		svc->setSettings(ss);
		if(!dir.isEmpty()) {
			if(!QDir().mkpath(dir))
				return false;
		}
	}
	return true;
}

void EmmaClientWidget::onBtExportFinishTxtClicked()
{
	EmmaClient *svc = service();
	if(svc) {
		saveSettings();
		svc->exportFinishRacomTxt();
	}
}

void EmmaClientWidget::onBtExportStartListTxtClicked()
{
	EmmaClient *svc = service();
	if(svc) {
		saveSettings();
		if (svc->settings().startExportType() == EmmaClientSettings::StartExportType::CSV)
			svc->exportStartListRacomCsv();
		else
			svc->exportStartListRacomTxt();
	}
}

void EmmaClientWidget::onBtExportResultsXml30Clicked()
{
	EmmaClient *svc = service();
	if(svc) {
		saveSettings();
		svc->exportResultsIofXml3();
	}
}

void EmmaClientWidget::onBtExportStartListXml30Clicked()
{
	EmmaClient *svc = service();
	if(svc) {
		saveSettings();
		svc->exportStartListIofXml3();
	}
}
}

