#include "receiptswidget.h"
#include "ui_receiptswidget.h"
#include "receiptsplugin.h"
#include "receiptssettings.h"

#include <quickevent/gui/og/itemdelegate.h>

#include <quickevent/core/og/timems.h>
#include <quickevent/gui/og/sqltablemodel.h>
#include <quickevent/core/si/siid.h>

#include <qf/gui/action.h>
#include <qf/gui/framework/application.h>
#include <qf/gui/framework/partwidget.h>
#include <qf/gui/framework/mainwindow.h>
#include <qf/gui/menubar.h>
#include <qf/gui/toolbar.h>

#include <qf/core/log.h>
#include <qf/core/assert.h>
#include <qf/core/exception.h>
#include <qf/core/sql/connection.h>
#include <qf/core/sql/query.h>
#include <qf/core/sql/dbenum.h>
#include <qf/gui/model/sqltablemodel.h>
#include <plugins/Event/src/eventplugin.h>
#include <quickevent/core/si/readcard.h>
#include <plugins/CardReader/src/cardreaderplugin.h>

#include <QSettings>
#include <QFile>
#include <QTextStream>
#include <QComboBox>
#include <QLabel>
#include <QMetaObject>
#include <QJSValue>
#include <QPrinterInfo>
#include <QTimer>
#include <QDirIterator>

namespace qfs = qf::core::sql;
namespace qfw = qf::gui;
using qf::gui::framework::getPlugin;
using Event::EventPlugin;
using Receipts::ReceiptsPlugin;

ReceiptsWidget::ReceiptsWidget(QWidget *parent) :
	Super(parent),
	ui(new Ui::ReceiptsWidget)
{
	ui->setupUi(this);
	//ui->cbxDirectPrinters->setCurrentIndex(2);
	createActions();

	{
		ui->tblPrintJobsTB->setTableView(ui->tblCards);

		ui->tblCards->setEditRowsMenuSectionEnabled(false);
		ui->tblCards->setDirtyRowsMenuSectionEnabled(false);
		ui->tblCards->setPersistentSettingsId(ui->tblCards->objectName());
		//ui->tblPrintJobs->setRowEditorMode(qfw::TableView::EditRowsMixed);
		ui->tblCards->setInlineEditSaveStrategy(qfw::TableView::OnEditedValueCommit);
		ui->tblCards->setItemDelegate(new quickevent::gui::og::ItemDelegate(ui->tblCards));
		auto m = new quickevent::gui::og::SqlTableModel(this);

		m->addColumn("cards.id", "id").setReadOnly(true);
		m->addColumn("cards.siId", tr("SI")).setReadOnly(true).setCastType(qMetaTypeId<quickevent::core::si::SiId>());
		m->addColumn("classes.name", tr("Class"));
		m->addColumn("competitorName", tr("Name"));
		m->addColumn("competitors.registration", tr("Reg"));
		m->addColumn("runs.startTimeMs", tr("Start")).setCastType(qMetaTypeId<quickevent::core::og::TimeMs>());
		m->addColumn("runs.timeMs", tr("Time")).setCastType(qMetaTypeId<quickevent::core::og::TimeMs>());
		/*
		qfm::SqlTableModel::ColumnDefinition::DbEnumCastProperties status_props;
		status_props.setGroupName("runs.status");
		m->addColumn("runs.status", tr("Status"))
				.setCastType(qMetaTypeId<qf::core::sql::DbEnum>(), status_props);
		*/
		m->addColumn("cards.printerConnectionId", tr("printer"));

		ui->tblCards->setTableModel(m);
		m_cardsModel = m;
	}

	ui->tblCards->setContextMenuPolicy(Qt::CustomContextMenu);
	connect(ui->tblCards, &qfw::TableView::customContextMenuRequested, this, &ReceiptsWidget::onCustomContextMenuRequest);
	connect(ui->btPrintNew, &QAbstractButton::clicked, this, &ReceiptsWidget::onPrintNewClicked);

	QTimer::singleShot(0, this, &ReceiptsWidget::lazyInit);
}

ReceiptsWidget::~ReceiptsWidget()
{
	delete ui;
}

void ReceiptsWidget::lazyInit()
{
}

void ReceiptsWidget::settleDownInPartWidget(::PartWidget *part_widget) const
{
	connect(part_widget, &::PartWidget::resetPartRequest, this, &ReceiptsWidget::reset);
	connect(part_widget, &::PartWidget::reloadPartRequest, this, &ReceiptsWidget::reload);

	connect(getPlugin<EventPlugin>(), &Event::EventPlugin::dbEventNotify, this, &ReceiptsWidget::onDbEventNotify, Qt::QueuedConnection);
}

void ReceiptsWidget::reset()
{
	if(!getPlugin<EventPlugin>()->isEventOpen()) {
		m_cardsModel->clearRows();
		return;
	}
	reload();
}

void ReceiptsWidget::reload()
{
	bool is_relays = getPlugin<EventPlugin>()->eventConfig()->isRelays();
	int current_stage = currentStageId();
	qfs::QueryBuilder qb;
	qb.select2("cards", "id, siId, printerConnectionId")
			.select2("runs", "startTimeMs, timeMs")
			.select2("competitors", "registration")
			.select2("classes", "name")
			.select("COALESCE(lastName, '') || ' ' || COALESCE(firstName, '') AS competitorName")
			.from("cards")
			.join("cards.runId", "runs.id")
			.join("runs.competitorId", "competitors.id")
			.where("cards.stageId=" QF_IARG(current_stage))
			.orderBy("cards.id DESC");
	if(is_relays) {
		qb.join("runs.relayId", "relays.id");
		qb.join("relays.classId", "classes.id");
	}
	else {
		qb.join("competitors.classId", "classes.id");
	}
	m_cardsModel->setQueryBuilder(qb, false);
	m_cardsModel->reload();
}

void ReceiptsWidget::onDbEventNotify(const QString &domain, int connection_id, const QVariant &data)
{
	Q_UNUSED(connection_id)
	Q_UNUSED(data)
	if(domain == QLatin1String(Event::EventPlugin::DBEVENT_CARD_READ)) {
		int card_id = data.toInt();
		onCardRead(connection_id, card_id);
	}
}

void ReceiptsWidget::createActions()
{
}

int ReceiptsWidget::currentStageId()
{
	return getPlugin<EventPlugin>()->currentStageId();
}

void ReceiptsWidget::onCardRead(int connection_id, int card_id)
{
	if(isAutoPrintEnabled()) {
		if(!thisReaderOnly() || connection_id == currentConnectionId()) {
			printReceipt(card_id);
		}
	}
	loadNewCards();
}

void ReceiptsWidget::printNewCards()
{
	int connection_id = currentConnectionId();
	QF_ASSERT(connection_id > 0, "Cannot get SQL connection id", return);
	int current_stage = currentStageId();
	ReceiptsSettings settings;
	qf::core::sql::Query q;
	QString qs = "SELECT id FROM cards"
			" WHERE printerConnectionId IS NULL"
			" AND stageId=" QF_IARG(current_stage);
	if(settings.isThisReaderOnly()) {
		qs += " AND readerConnectionId=" QF_IARG(connection_id);
	}
	qs += " ORDER BY id DESC";
	q.execThrow(qs);
	while(q.next()) {
		int card_id = q.value(0).toInt();
		if(!printReceipt(card_id))
			break;
	}
}

void ReceiptsWidget::loadNewCards()
{
	m_cardsModel->reloadInserts(QStringLiteral("cards.id"));
}

void ReceiptsWidget::onPrintNewClicked()
{
	printNewCards();
	loadNewCards();
}

void ReceiptsWidget::onCustomContextMenuRequest(const QPoint &pos)
{
	qfLogFuncFrame();
	int card_id = ui->tblCards->tableRow().value("cards.id").toInt();
	quickevent::core::si::ReadCard read_card = getPlugin<CardReader::CardReaderPlugin>()->readCard(card_id);
	bool has_runner = read_card.runId_isset() && read_card.runId() > 0;
	QAction a_print_receipts(tr("Print receipts for selected rows"), nullptr);
	QAction a_show_receipt(tr("Show receipt"), nullptr);
	a_show_receipt.setEnabled(has_runner);
	QAction a_show_card(tr("Show card data"), nullptr);
	QList<QAction*> lst;
	lst << &a_print_receipts;
	lst << &a_show_receipt;
	lst << &a_show_card;
	QAction *a = QMenu::exec(lst, ui->tblCards->viewport()->mapToGlobal(pos));
	if(a == &a_print_receipts) {
		printSelectedCards();
	}
	else if(a == &a_show_receipt) {
		getPlugin<ReceiptsPlugin>()->previewReceipt(card_id);
	}
	else if(a == &a_show_card) {
		getPlugin<ReceiptsPlugin>()->previewCard(card_id);
	}
}

void ReceiptsWidget::printSelectedCards()
{
	for(int i : ui->tblCards->selectedRowsIndexes()) {
		int card_id = ui->tblCards->tableRow(i).value("cards.id").toInt();
		QF_ASSERT(card_id > 0, "Bad card ID", return);
		printReceipt(card_id);
	}
}

bool ReceiptsWidget::printReceipt(int card_id)
{
	int connection_id = currentConnectionId();
	QF_ASSERT(connection_id > 0, "Cannot get SQL connection id", return false);
	bool ok = false;
	qf::core::sql::Query q;
	if(q.execThrow("SELECT runId FROM cards WHERE id=" QF_IARG(card_id))) {
		if(q.next()) {
			int run_id = q.value(0).toInt();
			if(run_id > 0)
				ok = getPlugin<ReceiptsPlugin>()->printReceipt(card_id);
			else {
				ReceiptsSettings settings;
				auto runner_not_found = settings.whenRunnerNotFoundPrintEnum();
				if (runner_not_found == ReceiptsSettings::WhenRunnerNotFoundPrint::ErrorInfo)
					ok = getPlugin<ReceiptsPlugin>()->printError(card_id,"error.qml");
				else if (runner_not_found == ReceiptsSettings::WhenRunnerNotFoundPrint::ErrorInfoLong)
					ok = getPlugin<ReceiptsPlugin>()->printError(card_id,"errorlong.qml");
				else
					ok = getPlugin<ReceiptsPlugin>()->printCard(card_id);
			}
		}
	}
	if(ok) {
		markAsPrinted(connection_id, card_id);
	}
	return ok;
}

void ReceiptsWidget::markAsPrinted(int connection_id, int card_id)
{
	QString qs = "UPDATE cards SET printerConnectionId=" QF_IARG(connection_id)
			" WHERE id = " QF_IARG(card_id);
	qf::core::sql::Query q;
	q.execThrow(qs);
}

bool ReceiptsWidget::isAutoPrintEnabled()
{
	ReceiptsSettings settings;
	return settings.isAutoPrint();
}

int ReceiptsWidget::currentConnectionId()
{
	return qf::core::sql::Connection::forName().connectionId();
}

bool ReceiptsWidget::thisReaderOnly()
{
	ReceiptsSettings settings;
	return settings.isThisReaderOnly();
}
