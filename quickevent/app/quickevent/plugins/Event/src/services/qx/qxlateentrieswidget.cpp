#include "qxlateentrieswidget.h"
#include "ui_qxlateentrieswidget.h"

#include "qxeventservice.h"
#include "lateentrydialog.h"
#include "runchange.h"

#include <plugins/Event/src/eventplugin.h>
#include <plugins/Runs/src/runswidget.h>

#include <qf/gui/framework/mainwindow.h>
#include <qf/gui/framework/application.h>
#include <qf/gui/model/sqltablemodel.h>
#include <qf/core/log.h>
#include <qf/core/sql/query.h>
#include <qf/core/sql/qxrecchng.h>
#include <qf/core/sql/qxsql.h>

#include <QMenu>
#include <QJsonDocument>
#include <QBrush>
#include <QColor>
#include <qmessagebox.h>

namespace qfm = qf::gui::model;
namespace qfs = qf::core::sql;
namespace qfw = qf::gui;
using qf::gui::framework::getPlugin;

namespace Event::services::qx {

namespace {
enum Columns {
	col_id = 0,
	col_stage_id,
	col_data_type,
	col_foreign_id,
	col_foreign_table,
	col_data,
	col_orig_data,
	col_status,
	col_status_message,
	col_user_id,
	col_created,
	col_lock_number,
	col_COUNT
};

constexpr auto COL_ID = "id";
constexpr auto COL_STAGE_ID = "stage_id";
constexpr auto COL_DATA_TYPE = "data_type";
constexpr auto COL_FOREIGN_ID = "foreign_id";
constexpr auto COL_FOREIGN_TABLE = "foreign_table";
constexpr auto COL_DATA = "data";
constexpr auto COL_ORIG_DATA = "orig_data";
constexpr auto COL_STATUS = "status";
constexpr auto COL_STATUS_MESSAGE = "status_message";
constexpr auto COL_USER_ID = "user_id";
constexpr auto COL_CREATED = "created";
constexpr auto COL_LOCK_NUMBER = "lock_number";

class LateEntriesModel : public qfm::SqlTableModel
{
public:
	using qfm::SqlTableModel::SqlTableModel;

	QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override
	{
		if (role == Qt::BackgroundRole) {
			if (index.column() == col_data) {
				auto data = index.data().toString();
				auto qxdata = QxChangeData::fromJson(data);
				if (auto le = qxdata.lateEntry; le.has_value()) {
					if (le.value().paid.value_or(false)) {
						return QColor("lightgreen");
					}
				}
			}
			else if (index.column() == col_status) {
				auto status = qxChangeStatusFromString(index.data().toString());
				switch (status) {
				case QxChangeStatus::Pending: return {};
				case QxChangeStatus::Accepted: return QColor("lightgreen");
				case QxChangeStatus::Rejected: return QColor("salmon");
				}
			}
		}
		return qfm::SqlTableModel::data(index, role);
	}
};
} // namespace

QxLateEntriesWidget::QxLateEntriesWidget(QWidget *parent) :
	QWidget(parent),
	ui(new Ui::QxLateEntriesWidget)
{
	ui->setupUi(this);

	ui->tableView->setReadOnly(true);
	ui->tableView->setContextMenuPolicy(Qt::CustomContextMenu);
	connect(ui->tableView, &qfw::TableView::customContextMenuRequested, this, &QxLateEntriesWidget::onTableCustomContextMenuRequest);
	connect(ui->tableView, &qfw::TableView::doubleClicked, this, &QxLateEntriesWidget::onTableDoubleClicked);

	ui->tableView->setPersistentSettingsId("tblQxLateEntries");
	ui->tableView->setInsertRowEnabled(false);
	ui->tableView->setCloneRowEnabled(false);
	ui->tableView->setRemoveRowEnabled(false);
	ui->tableView->setDirtyRowsMenuSectionEnabled(false);

	ui->toolbar->setTableView(ui->tableView);
	m_model = new LateEntriesModel(this);
	//m->setObjectName("classes.classesModel");

	m_model->clearColumns(col_COUNT);
	m_model->setColumn(col_id, qf::gui::model::TableModel::ColumnDefinition(COL_ID).setReadOnly(true).setAlignment(Qt::AlignRight));
	m_model->setColumn(col_stage_id, qf::gui::model::TableModel::ColumnDefinition(COL_STAGE_ID).setReadOnly(true).setAlignment(Qt::AlignRight));
	m_model->setColumn(col_data_type, qf::gui::model::SqlTableModel::ColumnDefinition(COL_DATA_TYPE).setReadOnly(true));
	m_model->setColumn(col_foreign_id, qf::gui::model::SqlTableModel::ColumnDefinition(COL_FOREIGN_ID).setReadOnly(true).setAlignment(Qt::AlignRight));
	m_model->setColumn(col_foreign_table, qf::gui::model::SqlTableModel::ColumnDefinition(COL_FOREIGN_TABLE).setReadOnly(true));
	m_model->setColumn(col_data, qf::gui::model::SqlTableModel::ColumnDefinition(COL_DATA).setReadOnly(true));
	m_model->setColumn(col_orig_data, qf::gui::model::SqlTableModel::ColumnDefinition(COL_ORIG_DATA).setReadOnly(true));
	m_model->setColumn(col_status, qf::gui::model::SqlTableModel::ColumnDefinition(COL_STATUS).setReadOnly(true));
	m_model->setColumn(col_status_message, qf::gui::model::SqlTableModel::ColumnDefinition(COL_STATUS_MESSAGE).setReadOnly(true));
	m_model->setColumn(col_user_id, qf::gui::model::SqlTableModel::ColumnDefinition(COL_USER_ID).setReadOnly(true));
	m_model->setColumn(col_created, qf::gui::model::SqlTableModel::ColumnDefinition(COL_CREATED).setReadOnly(true));
	m_model->setColumn(col_lock_number, qf::gui::model::SqlTableModel::ColumnDefinition(COL_LOCK_NUMBER).setReadOnly(true));
	ui->tableView->setTableModel(m_model);

	ui->tableView->setColumnHidden(col_stage_id, true);

	showMessage(tr("QxEvent service is disabled"), true);
	setEnabled(false);

	auto *svc = qxEventService();
	connect(svc, &Service::statusChanged, this, [this](Service::Status new_status){
		switch (new_status) {
		case Service::Status::Unknown:
		case Service::Status::Stopped:
			showMessage(tr("QxEvent service is disabled"), true);
			setEnabled(false);
			break;
			case Service::Status::Running:
			showMessage({});
			setEnabled(true);
			reload();
			break;
		}
	});

	{
		auto *lst = ui->lstType;
		lst->addItem("All");
		lst->addItem("RunUpdateRequest");
		lst->addItem("RunUpdated");
		lst->addItem("OcChange");
		lst->addItem("RadioPunch");
		lst->addItem("CardReadout");
		lst->setCurrentIndex(0);
		connect(lst, &QComboBox::currentIndexChanged, this, &QxLateEntriesWidget::reload);
	}
	connect(ui->chkPending, &QCheckBox::checkStateChanged, this, &QxLateEntriesWidget::reload);
	connect(ui->chkAccepted, &QCheckBox::checkStateChanged, this, &QxLateEntriesWidget::reload);
	connect(ui->chkRejected, &QCheckBox::checkStateChanged, this, &QxLateEntriesWidget::reload);

	connect(ui->btAll, &QPushButton::clicked, this, [this]() {
		QSignalBlocker sb1(ui->lstType);
		ui->lstType->setCurrentIndex(0);

		QSignalBlocker sb3(ui->chkPending);
		ui->chkPending->setChecked(true);
		QSignalBlocker sb5(ui->chkAccepted);
		ui->chkAccepted->setChecked(true);
		QSignalBlocker sb6(ui->chkRejected);
		ui->chkRejected->setChecked(true);

		reload();
	});
	connect(qf::gui::framework::Application::instance()->qxSql(), &qf::core::sql::QxSql::recChng, this, &QxLateEntriesWidget::onQxRecChng, Qt::QueuedConnection);
}

QxLateEntriesWidget::~QxLateEntriesWidget()
{
	delete ui;
}

void QxLateEntriesWidget::onVisibleChanged(bool is_visible)
{
	if (is_visible && isEnabled()) {
		reload();
	}
}

QxEventService *QxLateEntriesWidget::qxEventService()
{
	auto *svc = qobject_cast<QxEventService*>(Event::services::Service::serviceByName(QxEventService::serviceId()));
	Q_ASSERT(svc);
	return svc;
}

void QxLateEntriesWidget::resizeColumns()
{
	auto *tv = ui->tableView;
	tv->horizontalHeader()->resizeSections(QHeaderView::ResizeToContents);
}

void QxLateEntriesWidget::showMessage(const QString &msg, bool is_error)
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

void QxLateEntriesWidget::reload()
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
	if (ui->chkPending->isChecked()) {
		status_cond_list << "status='Pending'";
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

void QxLateEntriesWidget::applyQxChange(int sql_id)
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

void QxLateEntriesWidget::onTableCustomContextMenuRequest(const QPoint &pos)
{
	QAction a_edit_competitor(tr("Edit competitor"), nullptr);
	QList<QAction*> lst;
	lst << &a_edit_competitor;
	QAction *a = QMenu::exec(lst, ui->tableView->viewport()->mapToGlobal(pos));
	if(a == &a_edit_competitor) {
		if (auto ix = ui->tableView->indexAt(pos); ix.isValid()) {
			if (auto t = ix.sibling(ix.row(), col_foreign_table).data().toString(); t == "runs") {
				auto run_id = ix.sibling(ix.row(), col_foreign_id).data().toInt();
				qf::core::sql::Query q;
				q.execThrow(QStringLiteral("SELECT competitorId FROM runs WHERE id=%1")
							.arg(run_id)
							);
				if (q.next()) {
					auto competitor_id = q.value(0).toInt();
					RunsWidget::showEditCompetitorDialog(competitor_id, this);
					return;
				}
			}
		}
		QMessageBox::information(this, tr("Edit competitor"), tr("No competitor found for this run."));
	}
}

void QxLateEntriesWidget::onTableDoubleClicked(const QModelIndex &ix)
{
	auto row = ui->tableView->toTableModelIndex(ix).row();
	auto data = m_model->value(row, COL_DATA).toString();
	auto qxdata = QxChangeData::fromJson(data);
	if (qxdata.lateEntry.has_value()) {
		auto status = m_model->value(row, COL_STATUS).toString();
		auto status_message = m_model->value(row, COL_STATUS_MESSAGE).toString();
		auto change_id = m_model->value(row, COL_ID).toInt();
		auto stage_id = m_model->value(row, COL_STAGE_ID).toInt();
		LateEntryDialog dlg(change_id, stage_id, qxdata.lateEntry.value(), status, status_message, this);
		dlg.exec();
		ui->tableView->reloadRow(row);
	}
}

void QxLateEntriesWidget::onQxRecChng(const qf::core::sql::QxRecChng &recchng, QObject *source)
{
	if (recchng.table == "qxchanges" && recchng.record.contains("status")) {
		// status affect filter, cannot be solved by applyQxRecChng
		m_model->reload();
		return;
	}
	m_model->applyQxRecChng(recchng, source);
}

}
