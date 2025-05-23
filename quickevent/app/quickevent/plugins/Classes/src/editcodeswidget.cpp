#include "editcodeswidget.h"
#include "ui_editcodeswidget.h"

#include <qf/core/model/sqltablemodel.h>

namespace qfm = qf::core::model;
namespace qfs = qf::core::sql;

EditCodesWidget::EditCodesWidget(QWidget *parent)
	: Super(parent)
	, ui(new Ui::EditCodesWidget)
{
	setTitle(tr("Codes"));
	setPersistentSettingsId("EditCodesWidget");
	ui->setupUi(this);
	{
		ui->tableView->setPersistentSettingsId("tableView");
		//ui->tableView->setEditRowsSectionEnabled(false);
		ui->tableView->setDirtyRowsMenuSectionEnabled(false);
		ui->tableViewTB->setTableView(ui->tableView);
		auto *m = new qfm::SqlTableModel(this);
		//m->setObjectName("classes.classesModel");
		m->addColumn("id").setReadOnly(true);
		//m->addColumn("codes.type", tr("Type", "control type")).setToolTip(tr("Control type"));
		m->addColumn("codes.code", tr("Code"));
		m->addColumn("codes.altCode", tr("Alt")).setToolTip(tr("Code alternative"));
		m->addColumn("codes.note", tr("Note"));
		m->addColumn("codes.outOfOrder", tr("Out of order")).setToolTip(tr("Out of order"));
		m->addColumn("codes.radio", tr("Radio"));
		m->addColumn("codes.longitude", tr("Long")).setToolTip(tr("Longitude"));
		m->addColumn("codes.latitude", tr("Lat")).setToolTip(tr("Latitude"));
		ui->tableView->setTableModel(m);
		m_tableModel = m;
	}
	{
		qfs::QueryBuilder qb;
		qb.select2("codes", "*")
				.from("codes")
				.orderBy("codes.code");//.limit(10);
		m_tableModel->setQueryBuilder(qb, false);
		m_tableModel->reload();
	}
}

EditCodesWidget::~EditCodesWidget()
{
	delete ui;
}
