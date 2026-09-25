#include "punchingtestservicewidget.h"
#include "ui_punchingtestservicewidget.h"
#include "punchingtestservice.h"

#include "../service.h"
#include "../../eventplugin.h"

#include <qf/gui/framework/mainwindow.h>
#include <qf/core/assert.h>

#include <QDialog>

using qf::gui::framework::getPlugin;

namespace Event::services {

PunchingTestServiceWidget::PunchingTestServiceWidget(QWidget *parent)
	: Super(parent)
	, ui(new Ui::PunchingTestServiceWidget)
{
	setPersistentSettingsId("PunchingTestServiceWidget");
	ui->setupUi(this);
	ui->grpRelays->setVisible(getPlugin<EventPlugin>()->eventConfig().isRelays());

	PunchingTestService *svc = service();
	if (svc) {
		PunchingTestServiceSettings ss = svc->settings();
		ui->edPunchInterval->setValue(ss.punchInterval());
		ui->edUnknownCardRate->setValue(ss.unknownCardRate());
		ui->edMissingStartRate->setValue(ss.missingStartRate());
		ui->edMissingFinishRate->setValue(ss.missingFinishRate());
		ui->edExtraPunchRate->setValue(ss.extraPunchRate());
		ui->edBadCheckRate->setValue(ss.badCheckRate());
		ui->edMispunchRate->setValue(ss.mispunchRate());
		ui->cbxShameStartEnabled->setChecked(ss.shameStartEnabled());
		ui->edShameStartOffsetMin->setValue(ss.shameStartOffsetMin());
	}
}

PunchingTestServiceWidget::~PunchingTestServiceWidget()
{
	delete ui;
}

bool PunchingTestServiceWidget::acceptDialogDone(int result)
{
	if (result == QDialog::Accepted)
		saveSettings();
	return true;
}

PunchingTestService *PunchingTestServiceWidget::service()
{
	auto *svc = qobject_cast<PunchingTestService*>(
		Service::serviceByName(PunchingTestService::serviceName()));
	QF_ASSERT(svc, PunchingTestService::serviceName() + " doesn't exist", return nullptr);
	return svc;
}

void PunchingTestServiceWidget::saveSettings()
{
	PunchingTestService *svc = service();
	if (svc) {
		PunchingTestServiceSettings ss = svc->settings();
		ss.setPunchInterval(ui->edPunchInterval->value());
		ss.setUnknownCardRate(ui->edUnknownCardRate->value());
		ss.setMissingStartRate(ui->edMissingStartRate->value());
		ss.setMissingFinishRate(ui->edMissingFinishRate->value());
		ss.setExtraPunchRate(ui->edExtraPunchRate->value());
		ss.setBadCheckRate(ui->edBadCheckRate->value());
		ss.setMispunchRate(ui->edMispunchRate->value());
		ss.setShameStartEnabled(ui->cbxShameStartEnabled->isChecked());
		ss.setShameStartOffsetMin(ui->edShameStartOffsetMin->value());
		svc->setSettings(ss);
	}
}

} // namespace Event::services
