#include "printawardsoptionsdialogwidget.h"
#include "ui_printawardsoptionsdialogwidget.h"

#include <awarddesigner/awarddesign.h>
#include <awarddesigner/awarddesignerdialog.h>

#include <quickevent/gui/reportoptionsdialog.h>

#include <qf/gui/framework/mainwindow.h>
#include <qf/core/log.h>

PrintAwardsOptionsDialogWidget::PrintAwardsOptionsDialogWidget(QWidget *parent)
	: Super(parent)
	, ui(new Ui::PrintAwardsOptionsDialogWidget)
{
	setPersistentSettingsId(objectName());
	ui->setupUi(this);
	ui->edNumPlaces->setValue(3);

	refreshTemplateList();

	connect(ui->btnDesigner, &QPushButton::clicked, this, &PrintAwardsOptionsDialogWidget::onDesignerClicked);
}

PrintAwardsOptionsDialogWidget::~PrintAwardsOptionsDialogWidget()
{
	delete ui;
}

void PrintAwardsOptionsDialogWidget::refreshTemplateList()
{
	QString currentData = ui->edReportPath->currentData().toString();
	ui->edReportPath->clear();

	for (const auto &[name, path] : AwardDesigner::Design::listAwards(QStringLiteral("Runs")).asKeyValueRange()) {
		ui->edReportPath->addItem(name, path);
	}

	if (!currentData.isEmpty()) {
		int ix = ui->edReportPath->findData(currentData);
		if (ix >= 0)
			ui->edReportPath->setCurrentIndex(ix);
	}
}

QVariantMap PrintAwardsOptionsDialogWidget::printOptions() const
{
	QVariantMap ret;
	if (ui->edReportPath->currentIndex() >= 0) {
		ret["numPlaces"] = ui->edNumPlaces->value();
		ret["stageId"]   = ui->edStageNumber->value();
		ret["reportPath"] = ui->edReportPath->currentData().toString();
	}

	quickevent::gui::ReportOptionsDialog::Options filterOpts;
	filterOpts.setUseClassFilter(ui->grpClassFilter->isChecked());
	filterOpts.setClassFilter(ui->edFilter->text().trimmed());
	filterOpts.setInvertClassFilter(ui->chkClassFilterDoesntMatch->isChecked());
	int ft = ui->btWildCard->isChecked()  ? 0
	       : ui->btRegExp->isChecked()    ? 1
	       : 2;
	filterOpts.setClassFilterType(ft);
	ret["classFilter"]       = quickevent::gui::ReportOptionsDialog::sqlWhereExpression(filterOpts, ui->edStageNumber->value());
	ret["classFilterText"]   = filterOpts.classFilter();
	ret["classFilterType"]   = filterOpts.classFilterType();
	ret["useClassFilter"]    = filterOpts.isUseClassFilter();
	ret["invertClassFilter"] = filterOpts.isInvertClassFilter();

	return ret;
}

void PrintAwardsOptionsDialogWidget::setPrintOptions(const QVariantMap &opts)
{
	ui->edNumPlaces->setValue(opts.value("numPlaces", 3).toInt());
	ui->edStageNumber->setValue(opts.value("stageId").toInt());
	QString report_path = opts.value("reportPath").toString();
	if (!report_path.isEmpty()) {
		int ix = ui->edReportPath->findData(report_path);
		if (ix >= 0)
			ui->edReportPath->setCurrentIndex(ix);
	}

	ui->grpClassFilter->setChecked(opts.value("useClassFilter", false).toBool());
	ui->edFilter->setText(opts.value("classFilterText").toString());
	ui->chkClassFilterDoesntMatch->setChecked(opts.value("invertClassFilter", false).toBool());
	int ft = opts.value("classFilterType", 0).toInt();
	if (ft == 1)      ui->btRegExp->setChecked(true);
	else if (ft == 2) ui->btClassNames->setChecked(true);
	else              ui->btWildCard->setChecked(true);
}

void PrintAwardsOptionsDialogWidget::onDesignerClicked()
{
	AwardDesigner::Design design;
	QString name = ui->edReportPath->currentData().toString();
	design = AwardDesigner::Design::loadFile(name);

	AwardDesignerDialog dlg(AwardDesigner::runsFields(), AwardDesigner::Design::defaultRunsDesign(), this);
	if (design.isValid())
		dlg.loadDesign(design);
	dlg.exec();

	refreshTemplateList();

	QString savedName = dlg.designName();
	if (!savedName.isEmpty()) {
		int ix = ui->edReportPath->findData(savedName);
		if (ix >= 0)
			ui->edReportPath->setCurrentIndex(ix);
	}
}
