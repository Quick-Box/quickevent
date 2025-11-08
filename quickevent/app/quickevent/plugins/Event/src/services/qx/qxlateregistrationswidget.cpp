#include "qxlateregistrationswidget.h"
#include "ui_qxlateregistrationswidget.h"

#include "qxeventservice.h"
#include "runchangedialog.h"
#include "runchange.h"

#include <plugins/Event/src/eventplugin.h>

#include <qf/gui/framework/mainwindow.h>
#include <qf/gui/model/sqltablemodel.h>
#include <qf/core/log.h>
#include <qf/core/sql/query.h>

#include <QMenu>
#include <QJsonDocument>

namespace qfm = qf::gui::model;
namespace qfs = qf::core::sql;
namespace qfw = qf::gui;
using qf::gui::framework::getPlugin;

namespace Event::services::qx {

constexpr auto COL_ID = "id";
constexpr auto COL_CHANGE_ID = "change_id";
constexpr auto COL_DATA_TYPE = "data_type";
constexpr auto COL_DATA_ID = "data_id";
constexpr auto COL_DATA = "data";
constexpr auto COL_ORIG_DATA = "orig_data";
constexpr auto COL_STATUS = "status";
constexpr auto COL_STATUS_MESSAGE = "status_message";
constexpr auto COL_SOURCE = "source";
constexpr auto COL_USER_ID = "user_id";
constexpr auto COL_CREATED = "created";
constexpr auto COL_LOCK_NUMBER = "lock_number";

constexpr auto STATUS_PENDING = "Pending";
constexpr auto STATUS_LOCKED = "Locked";

constexpr auto DATA_TYPE_RUN_UPDATE_REQUEST = "RunUpdateRequest";

constexpr auto SOURCE_WWW = "www";

QxLateRegistrationsWidget::QxLateRegistrationsWidget(QWidget *parent) :
	QWidget(parent),
	ui(new Ui::QxLateRegistrationsWidget)
{
	ui->setupUi(this);

	ui->tableView->setReadOnly(true);
	ui->tableView->setContextMenuPolicy(Qt::CustomContextMenu);
	connect(ui->tableView, &qfw::TableView::customContextMenuRequested, this, &QxLateRegistrationsWidget::onTableCustomContextMenuRequest);
	connect(ui->tableView, &qfw::TableView::doubleClicked, this, &QxLateRegistrationsWidget::onTableDoubleClicked);

	ui->tableView->setPersistentSettingsId("tblQxLateRegistrations");
	ui->tableView->setInsertRowEnabled(false);
	ui->tableView->setCloneRowEnabled(false);
	ui->tableView->setRemoveRowEnabled(false);
	ui->tableView->setDirtyRowsMenuSectionEnabled(false);

	ui->toolbar->setTableView(ui->tableView);
	m_model = new qfm::SqlTableModel(this);
	//m->setObjectName("classes.classesModel");
	m_model->addColumn(COL_ID).setReadOnly(true).setAlignment(Qt::AlignLeft);
	m_model->addColumn(COL_STATUS, tr("Status"));
	m_model->addColumn(COL_DATA_TYPE, tr("Type"));
	m_model->addColumn(COL_DATA_ID, tr("Data ID")).setAlignment(Qt::AlignLeft);
	m_model->addColumn(COL_SOURCE, tr("Source"));
	m_model->addColumn(COL_USER_ID, tr("User"));
	m_model->addColumn(COL_STATUS_MESSAGE, tr("Status message"));
	m_model->addColumn(COL_CREATED, tr("Created"));
	m_model->addColumn(COL_CHANGE_ID, tr("Change ID"));
	m_model->addColumn(COL_LOCK_NUMBER, tr("Lock"));
	m_model->addColumn(COL_DATA, tr("Data"));//.setToolTip(tr("Locked for drawing"));
	m_model->addColumn(COL_ORIG_DATA, tr("Orig data"));//.setToolTip(tr("Locked for drawing"));
	ui->tableView->setTableModel(m_model);

	showMessage({});
	setEnabled(false);

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

	connect(getPlugin<EventPlugin>(), &Event::EventPlugin::dbEventNotify, this, &QxLateRegistrationsWidget::onDbEventNotify, Qt::QueuedConnection);

	{
		auto *lst = ui->lstType;
		lst->addItem("All");
		lst->addItem("RunUpdateRequest");
		lst->addItem("RunUpdated");
		lst->addItem("OcChange");
		lst->addItem("RadioPunch");
		lst->addItem("CardReadout");
		lst->setCurrentIndex(0);
		connect(lst, &QComboBox::currentIndexChanged, this, &QxLateRegistrationsWidget::reload);
	}
	connect(ui->chkNull, &QCheckBox::checkStateChanged, this, &QxLateRegistrationsWidget::reload);
	connect(ui->chkPending, &QCheckBox::checkStateChanged, this, &QxLateRegistrationsWidget::reload);
	connect(ui->chkLocked, &QCheckBox::checkStateChanged, this, &QxLateRegistrationsWidget::reload);
	connect(ui->chkAccepted, &QCheckBox::checkStateChanged, this, &QxLateRegistrationsWidget::reload);
	connect(ui->chkRejected, &QCheckBox::checkStateChanged, this, &QxLateRegistrationsWidget::reload);

	connect(ui->btAll, &QPushButton::clicked, this, [this]() {
		QSignalBlocker sb1(ui->lstType);
		ui->lstType->setCurrentIndex(0);

		QSignalBlocker sb2(ui->chkNull);
		ui->chkNull->setChecked(true);
		QSignalBlocker sb3(ui->chkPending);
		ui->chkPending->setChecked(true);
		QSignalBlocker sb4(ui->chkLocked);
		ui->chkLocked->setChecked(true);
		QSignalBlocker sb5(ui->chkAccepted);
		ui->chkAccepted->setChecked(true);
		QSignalBlocker sb6(ui->chkRejected);
		ui->chkRejected->setChecked(true);

		reload();
	});
}

QxLateRegistrationsWidget::~QxLateRegistrationsWidget()
{
	delete ui;
}

void QxLateRegistrationsWidget::onDbEventNotify(const QString &domain, int connection_id, const QVariant &payload)
{
	Q_UNUSED(connection_id)
	if(domain == QLatin1String(Event::EventPlugin::DBEVENT_QX_CHANGE_RECEIVED)) {
		int sql_id = payload.toInt();
		addQxChangeRow(sql_id);
	}
}

void QxLateRegistrationsWidget::onVisibleChanged(bool is_visible)
{
	if (is_visible && isEnabled()) {
		reload();
	}
}

QxEventService *QxLateRegistrationsWidget::service()
{
	auto *svc = qobject_cast<QxEventService*>(Event::services::Service::serviceByName(QxEventService::serviceId()));
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
			.orderBy("id");
	QStringList status_cond_list;
	if (ui->chkNull->isChecked()) {
		status_cond_list << "status IS NULL";
	}
	if (ui->chkPending->isChecked()) {
		status_cond_list << "status='Pending'";
	}
	if (ui->chkLocked->isChecked()) {
		status_cond_list << "status LIKE 'Locked%'";
	}
	if (ui->chkAccepted->isChecked()) {
		status_cond_list << "status='Accepted'";
	}
	if (ui->chkRejected->isChecked()) {
		status_cond_list << "status='Rejected'";
	}
	if (!status_cond_list.isEmpty()) {
		qb.where('(' + status_cond_list.join(" OR ") + ')');
	}
	if (auto ix = ui->lstType->currentIndex(); ix > 0) {
		auto data_type = ui->lstType->currentText();
		qb.where(QStringLiteral("data_type='%1'").arg(data_type));
	}
	qfDebug() << qb.toString();
	m_model->setQueryBuilder(qb, false);
	m_model->reload();
}

void QxLateRegistrationsWidget::addQxChangeRow(int sql_id)
{
	qfDebug() << "reloading qxchanges row id:" << sql_id << "col id:" << COL_ID;
	if(sql_id <= 0) {
		return;
	}

	auto qb = m_model->queryBuilder();
	qb.where(QStringLiteral("id=%1").arg(sql_id));
	qf::core::sql::Query q;
	q.execThrow(qb.toString());
	if (!q.next()) {
		// inserted row is filtered out
		return;
	}

	m_model->insertRow(0);
	m_model->setValue(0, COL_ID, sql_id);
	int cnt = m_model->reloadRow(0);
	if(cnt != 1) {
		qfWarning() << "Inserted qx change row id:" << sql_id << "reloaded in" << cnt << "instances.";
		return;
	}
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

void QxLateRegistrationsWidget::onTableCustomContextMenuRequest(const QPoint &pos)
{
	QAction a_neco(tr("Neco"), nullptr);
	QList<QAction*> lst;
	lst << &a_neco;
	QAction *a = QMenu::exec(lst, ui->tableView->viewport()->mapToGlobal(pos));
	if(a == &a_neco) {
		//printSelectedCards();
	}
}

void QxLateRegistrationsWidget::onTableDoubleClicked(const QModelIndex &ix)
{
	auto row = ix.row();
	auto data_type = m_model->value(row, COL_DATA_TYPE).toString();
	if (data_type != DATA_TYPE_RUN_UPDATE_REQUEST) {
		return;
	}
	auto source = m_model->value(row, COL_SOURCE).toString();
	if (source != SOURCE_WWW) {
		return;
	}
	auto change_id = m_model->value(row, COL_CHANGE_ID).toInt();
	auto status = m_model->value(row, COL_STATUS).toString();
	auto lock_number = m_model->value(row, COL_LOCK_NUMBER).toInt();
	auto data_id = m_model->value(row, COL_DATA_ID).toInt();
	auto data = m_model->value(row, COL_DATA).toString().toUtf8();
	auto change_rec = QJsonDocument::fromJson(data).toVariant().toMap();
	auto run_change = RunChange::fromVariantMap(change_rec.value(DATA_TYPE_RUN_UPDATE_REQUEST).toMap());
	if (status == STATUS_PENDING || status == STATUS_LOCKED) {
		RunChangeDialog dlg(change_id, data_id, lock_number, run_change, this);
		dlg.exec();
		ui->tableView->reloadRow(row);
	}
}

}

