#include "qxlateregistrationswidget.h"
#include "ui_qxlateregistrationswidget.h"

#include "qxclientservice.h"

#include <plugins/Event/src/eventplugin.h>
#include <plugins/Competitors/src/competitorsplugin.h>

#include <qf/qmlwidgets/framework/mainwindow.h>
#include <qf/core/model/sqltablemodel.h>
#include <qf/core/log.h>

namespace qfm = qf::core::model;
namespace qfs = qf::core::sql;
using qf::qmlwidgets::framework::getPlugin;

namespace Event::services::qx {

QxLateRegistrationsWidget::QxLateRegistrationsWidget(QWidget *parent) :
	QWidget(parent),
	ui(new Ui::QxLateRegistrationsWidget)
{
	ui->setupUi(this);

	ui->tableView->setPersistentSettingsId("tblQxLateRegistrations");
	ui->tableView->setInsertRowEnabled(true);
	ui->tableView->setCloneRowEnabled(false);
	ui->tableView->setRemoveRowEnabled(true);
	ui->tableView->setDirtyRowsMenuSectionEnabled(false);

	ui->toolbar->setTableView(ui->tableView);
	m_model = new qfm::SqlTableModel(this);
	//m->setObjectName("classes.classesModel");
	m_model->addColumn(COL_ID).setReadOnly(true).setAlignment(Qt::AlignLeft);
	m_model->addColumn(COL_DATA_TYPE, tr("Type"));
	m_model->addColumn(COL_DATA, tr("Data"));//.setToolTip(tr("Locked for drawing"));
	m_model->addColumn(COL_SOURCE, tr("Source"));
	m_model->addColumn(COL_RUN_ID, tr("Run")).setAlignment(Qt::AlignLeft);
	m_model->addColumn(COL_USER_ID, tr("User"));
	m_model->addColumn(COL_STATUS, tr("Status"));
	m_model->addColumn(COL_STATUS_MESSAGE, tr("Status message"));
	m_model->addColumn(COL_CREATED, tr("Created"));
	m_model->addColumn(COL_CROW_LOCK, tr("Locked"));
	ui->tableView->setTableModel(m_model);


	showMessage({});
	setEnabled(false);

	//connect(ui->btReload, &QAbstractButton::clicked, this, &QxLateRegistrationsWidget::reload);
	//connect(ui->btResizeColumns, &QAbstractButton::clicked, this, &QxLateRegistrationsWidget::resizeColumns);
	//connect(ui->btApply, &QAbstractButton::clicked, this, &QxLateRegistrationsWidget::applyCurrentChange);

	auto *svc = service();
	connect(svc, &Service::statusChanged, this, [this](Service::Status new_status){
		switch (new_status) {
		case Service::Status::Unknown:
		case Service::Status::Stopped:
			setEnabled(false);
			break;
			case Service::Status::Running:
			setEnabled(true);
			reload();
			break;
		}
	});

}

QxLateRegistrationsWidget::~QxLateRegistrationsWidget()
{
	delete ui;
}

void QxLateRegistrationsWidget::onDbEventNotify(const QString &domain, const QVariant &payload)
{
	Q_UNUSED(payload)
	if(domain == QLatin1String(Event::EventPlugin::DBEVENT_QX_CHANGE_RECEIVED)) {
		int change_id = payload.toInt();
		addQxChangeRow(change_id);
	}
}

void QxLateRegistrationsWidget::onVisibleChanged(bool is_visible)
{
	if (is_visible && isEnabled()) {
		reload();
	}
}

QxClientService *QxLateRegistrationsWidget::service()
{
	auto *svc = qobject_cast<QxClientService*>(Event::services::Service::serviceByName(QxClientService::serviceId()));
	Q_ASSERT(svc);
	return svc;
}

void QxLateRegistrationsWidget::resizeColumns()
{
	auto *tv = ui->tableView;
	tv->horizontalHeader()->resizeSections(QHeaderView::ResizeToContents);
}

void QxLateRegistrationsWidget::showMessage(const QString &msg, bool is_error)
{
	if (msg.isEmpty()) {
		ui->lblErrorMsg->hide();
	}
	else {
		if (is_error) {
			ui->lblErrorMsg->setStyleSheet("background: salmon");
		}
		else {
			ui->lblErrorMsg->setStyleSheet("background: lightgreen");
		}
		ui->lblErrorMsg->show();
	}
	ui->lblErrorMsg->setText(msg);
}

void QxLateRegistrationsWidget::reload()
{
	if(!isEnabled()) {
		return;
	}
	auto event_plugin = getPlugin<EventPlugin>();
	if(!getPlugin<EventPlugin>()->isEventOpen()) {
		return;
	}
	int stage_id = event_plugin->currentStageId();
	qfs::QueryBuilder qb;
	qb.select2("qxchanges", "*")
			.from("qxchanges")
			.where("stage_id=" + QString::number(stage_id))
			.orderBy("id");//.limit(10);
	qfDebug() << qb.toString();
	m_model->setQueryBuilder(qb, false);
	m_model->reload();
}

void QxLateRegistrationsWidget::addQxChangeRow(int change_sql_id)
{
	if(change_sql_id <= 0) {
		return;
	}
	m_model->insertRow(0);
	m_model->setValue(0, COL_ID, change_sql_id);
	int cnt = m_model->reloadRow(0);
	if(cnt != 1) {
		qfWarning() << "Inserted qx change row id:" << change_sql_id << "reloaded in" << cnt << "instances.";
		return;
	}
	ui->tableView->updateRow(0);
}


void QxLateRegistrationsWidget::applyCurrentChange()
{
	auto row = ui->tableView->currentIndex().row();
	if (row < 0) {
		return;
	}
	auto status = m_model->value(row, COL_STATUS).toString();
	if (status != STATUS_PENDING) {
		return;
	}
	//auto run_id = m_model->value(row, COL_RUN_ID).toInt();
	//int competitor_id = 0;
	// int result = getPlugin<Competitors::CompetitorsPlugin>()->editCompetitor(competitor_id, run_id == 0? Mode::Insert: Mode::Edit);

}

}

