#include "registrationswidget.h"
#include "ui_registrationswidget.h"
#include "eventplugin.h"

#include <qf/gui/framework/mainwindow.h>

#include <qf/gui/model/sqltablemodel.h>
#include <qf/core/sql/querybuilder.h>
#include <qf/core/sql/connection.h>
#include <qf/core/assert.h>

using qf::gui::framework::getPlugin;
using Event::EventPlugin;

RegistrationsWidget::RegistrationsWidget(QWidget *parent) :
	QWidget(parent),
	ui(new Ui::RegistrationsWidget)
{
	ui->setupUi(this);

	ui->tblRegistrationsTB->setTableView(ui->tblRegistrations);

	ui->tblRegistrations->setPersistentSettingsId("tblRegistrations");
	ui->tblRegistrations->setReadOnly(true);
	//ui->tblRegistrations->setInlineEditStrategy(qfw::TableView::OnCurrentFieldChange);
}

RegistrationsWidget::~RegistrationsWidget()
{
	delete ui;
}

void RegistrationsWidget::checkModel()
{
	qfLogFuncFrame();
	if(!isVisible())
		return;

	if(!ui->tblRegistrations->tableModel()) {
		qf::gui::model::SqlTableModel *reg_model = getPlugin<EventPlugin>()->registrationsModel();
		ui->tblRegistrations->setTableModel(reg_model);
		connect(reg_model, &qf::gui::model::SqlTableModel::reloaded, this, [this]() {
			ui->tblRegistrations->horizontalHeader()->resizeSections(QHeaderView::ResizeToContents);
		});
	}
}

qf::gui::TableView *RegistrationsWidget::tableView()
{
	return ui->tblRegistrations;
}

