#include "cardreaderwidget.h"
#include "ui_cardreaderwidget.h"
#include "cardreadersettings.h"

#include "cardreaderplugin.h"

#include <quickevent/gui/og/itemdelegate.h>
#include <quickevent/gui/audio/player.h>

#include <quickevent/core/codedef.h>
#include <quickevent/core/og/timems.h>
#include <quickevent/gui/og/sqltablemodel.h>
#include <quickevent/core/si/punchrecord.h>
#include <quickevent/core/si/readcard.h>
#include <quickevent/core/si/checkedcard.h>
#include <quickevent/core/si/siid.h>

#include <siut/sidevicedriver.h>
#include <siut/commport.h>
#include <siut/sicard.h>
#include <siut/sitask.h>

#include <qf/gui/action.h>
#include <qf/gui/framework/application.h>
#include <qf/gui/framework/partwidget.h>
#include <qf/gui/framework/mainwindow.h>
#include <qf/gui/combobox.h>
#include <qf/gui/dialogbuttonbox.h>
#include <qf/gui/htmlviewwidget.h>
#include <qf/gui/menubar.h>
#include <qf/gui/toolbar.h>
#include <qf/gui/dialogs/dialog.h>
#include <qf/gui/dialogs/messagebox.h>
#include <qf/gui/dialogs/filedialog.h>
#include <qf/gui/log.h>

#include <qf/core/assert.h>
#include <qf/core/exception.h>
#include <qf/core/sql/query.h>
#include <qf/core/sql/dbenum.h>
#include <qf/core/sql/transaction.h>
#include <qf/gui/model/sqltablemodel.h>
#include <qf/core/utils/settings.h>
#include <qf/core/utils/csvreader.h>
#include <qf/core/utils/htmlutils.h>
#include <plugins/Receipts/src/receiptsplugin.h>
#include <plugins/Event/src/eventplugin.h>
#include <plugins/Runs/src/findrunnerwidget.h>
// #include <plugins/Competitors/src/competitorsplugin.h>
#include <plugins/Runs/src/runflagsdialog.h>
#include <plugins/Runs/src/runsplugin.h>

#include <QSettings>
#include <QFile>
#include <QTextStream>
#include <QComboBox>
#include <QLabel>
#include <QCheckBox>
#include <QPushButton>
#include <QProgressDialog>
#include <QTimer>
#include <QSerialPortInfo>

namespace qfc = qf::core;
namespace qfs = qf::core::sql;
namespace qfw = qf::gui;
namespace qfd = qf::gui::dialogs;
using qf::gui::framework::getPlugin;
using Event::EventPlugin;
using CardReader::CardReaderPlugin;
using Receipts::ReceiptsPlugin;
// using Competitors::CompetitorsPlugin;
using Runs::RunsPlugin;

namespace {
class Model : public quickevent::gui::og::SqlTableModel
{
	Q_OBJECT
private:
	typedef quickevent::gui::og::SqlTableModel Super;
public:
	enum Columns {
		col_cards_id = 0,
		col_cards_siId,
		col_classes_name,
		col_competitorName,
		col_competitors_registration,
		col_competirors_bib,
		col_runs_startTimeMs,
		col_runs_timeMs,
		col_runs_finishTimeMs,
		col_runFlags,
		col_runs_cardLent,
		col_runs_cardLentTable,
		col_runs_cardReturned,
		col_cards_checkTime,
		col_cards_startTime,
		col_cards_finishTime,
		col_cards_runIdAssignError,
		col_COUNT
	};
public:
	explicit Model(QObject *parent);

	int columnCount(const QModelIndex &) const override { return col_COUNT; }
	QVariant data(const QModelIndex &index, int role) const Q_DECL_OVERRIDE;

	using Super::value;
	QVariant value(int row_ix, int column_ix) const Q_DECL_OVERRIDE;
};

Model::Model(QObject *parent)
	: Super(parent)
{
	setIdColumnName("cards.id");
	clearColumns(col_COUNT);
	setColumn(col_cards_id, ColumnDefinition("cards.id", "id").setReadOnly(true));
	setColumn(col_cards_siId, ColumnDefinition("cards.siId", tr("SI")).setReadOnly(true).setCastType(qMetaTypeId<quickevent::core::si::SiId>()));
	setColumn(col_classes_name, ColumnDefinition("classes.name", tr("Class")));
	setColumn(col_competitorName, ColumnDefinition("competitorName", tr("Name")));
	setColumn(col_competitors_registration, ColumnDefinition("competitors.registration", tr("Reg")));
	setColumn(col_competirors_bib, ColumnDefinition("competitors.startNumber", tr("Bib")));
	setColumn(col_runs_startTimeMs, ColumnDefinition("runs.startTimeMs", tr("Start")).setCastType(qMetaTypeId<quickevent::core::og::TimeMs>()).setReadOnly(true));
	setColumn(col_runs_timeMs, ColumnDefinition("runs.timeMs", tr("Time")).setCastType(qMetaTypeId<quickevent::core::og::TimeMs>()).setReadOnly(true));
	setColumn(col_runs_finishTimeMs, ColumnDefinition("runs.finishTimeMs", tr("Finish")).setCastType(qMetaTypeId<quickevent::core::og::TimeMs>()).setReadOnly(true));
	setColumn(col_runFlags, ColumnDefinition("runFlags", tr("Run flags")).setReadOnly(true));
	setColumn(col_runs_cardLent, ColumnDefinition("cardLent", tr("CR")).setToolTip(tr("Card rent")).setReadOnly(true).setCastType(qMetaTypeId<bool>()));
	setColumn(col_runs_cardLentTable, ColumnDefinition("cardLentTable", tr("CRT")).setToolTip(tr("Card in rent table")).setReadOnly(true).setCastType(qMetaTypeId<bool>()));
	setColumn(col_runs_cardReturned, ColumnDefinition("runs.cardReturned", tr("CRET")).setToolTip(tr("Card returned")));
	setColumn(col_cards_checkTime, ColumnDefinition("cards.checkTime", tr("CTIME")).setToolTip(tr("Card check time")).setReadOnly(true));
	setColumn(col_cards_startTime, ColumnDefinition("cards.startTime", tr("STIME")).setToolTip(tr("Card start time")).setReadOnly(true));
	setColumn(col_cards_finishTime, ColumnDefinition("cards.finishTime", tr("FTIME")).setToolTip(tr("Card finish time")).setReadOnly(true));
	setColumn(col_cards_runIdAssignError, ColumnDefinition("cards.runIdAssignError", tr("Error")).setToolTip(tr("Assign card to runner error")).setReadOnly(true));
}

QVariant Model::data(const QModelIndex &index, int role) const
{
	int col = index.column();
	if(role == Qt::BackgroundRole) {
		static auto C_RUNID = QStringLiteral("cards.runId");
		int run_id = tableRow(index.row()).value(C_RUNID).toInt();
		if(run_id == 0) {
			static auto c = QColor(Qt::red).lighter(150);
			return c;
		}
	}
	else if(role == Qt::DisplayRole) {
		if(col == col_cards_checkTime
		   || col == col_cards_startTime
		   || col == col_cards_finishTime )
		{
			int secs = value(index.row(), col).toInt();
			if(secs == 0xEEEE)
				return QString();
			int sec = secs % 60;
			secs /= 60;
			int min = secs % 60;
			secs /= 60;
			int hod = secs;
			QString s("%1:%2:%3");
			s = s.arg(hod, 2, 10, QChar('0'));
			s = s.arg(min, 2, 10, QChar('0'));
			s = s.arg(sec, 2, 10, QChar('0'));
			return s;
		}
	}
	return Super::data(index, role);
}


QVariant Model::value(int row_ix, int column_ix) const
{
	if(column_ix == col_runFlags) {
		qf::core::utils::TableRow row = tableRow(row_ix);
		bool is_disqualified = row.value(QStringLiteral("runs.disqualified")).toBool();
		bool mis_punch = row.value(QStringLiteral("runs.misPunch")).toBool();
		bool bad_check = row.value(QStringLiteral("runs.badCheck")).toBool();
		bool not_start = row.value(QStringLiteral("runs.notStart")).toBool();
		bool not_finish = row.value(QStringLiteral("runs.notFinish")).toBool();
		bool is_disqualified_by_organizer = row.value(QStringLiteral("runs.disqualifiedByOrganizer")).toBool();
		bool over_time = row.value(QStringLiteral("runs.overTime")).toBool();
		bool not_competing = row.value(QStringLiteral("runs.notCompeting")).toBool();
		QStringList sl;
		if(not_competing)
			sl << tr("NC", "NotCompeting");
		if(mis_punch)
			sl << tr("MP", "MisPunch");
		if(bad_check)
			sl << tr("BC", "BadCheck");
		if(not_start)
			sl << tr("DNS", "DidNotStart");
		if(not_finish)
			sl << tr("DNF", "DidNotFinish");
		if(is_disqualified_by_organizer)
			sl << tr("DO", "disqualifiedByOrganizer");
		if(over_time)
			sl << tr("OT", "OverTime");
		if(is_disqualified && !mis_punch && !bad_check && !not_start && !not_finish && !is_disqualified_by_organizer && !over_time)
			sl << tr("DSQ", "Disqualified");
		if(sl.isEmpty()) {
			return QStringLiteral("");
		}
		return sl.join(',');
	}
	return Super::value(row_ix, column_ix);
}

}

CardReaderWidget::CardReaderWidget(QWidget *parent)
	: Super(parent)
	, ui(new Ui::CardReaderWidget)
{
	ui->setupUi(this);

	ui->tblCards->setEditRowsMenuSectionEnabled(false);
	ui->tblCards->setPersistentSettingsId(ui->tblCards->objectName());

	createActions();

	{
		ui->btComPort->setToolTip(tr("Open COM to connect SI reader"));
		connect(ui->btComPort, &QPushButton::toggled, this, &CardReaderWidget::onOpenCommTriggered);
	}
	ui->lblConnectionInfo->setText(tr("SI station not connected"));
#ifdef QT_DEBUG
	{
		connect(ui->btTest, &QPushButton::clicked, this, &CardReaderWidget::onTestButtonClicked);
		connect(ui->btTestSound, &QPushButton::clicked, this, [this]() {
			audioPlayer()->playAlert(quickevent::gui::audio::Player::AlertKind::OperatorWakeUp);
		});
	}
#else
	ui->btTest->hide();
	ui->btTestSound->hide();
#endif

	connect(getPlugin<CardReaderPlugin>(), &CardReader::CardReaderPlugin::siTaskFinished, this, &CardReaderWidget::onSiTaskFinished);
	{
		ui->tblCardsTB->setTableView(ui->tblCards);

		ui->tblCards->setPersistentSettingsId("tblCards");
		ui->tblCards->setDirtyRowsMenuSectionEnabled(false);
		ui->tblCards->setRowEditorMode(qfw::TableView::EditRowsMixed);
		ui->tblCards->setInlineEditSaveStrategy(qfw::TableView::OnEditedValueCommit);
		ui->tblCards->setItemDelegate(new quickevent::gui::og::ItemDelegate(ui->tblCards));
		auto m = new Model(this);
		ui->tblCards->setTableModel(m);
		m_cardsModel = m;
	}
	ui->tblCards->setContextMenuPolicy(Qt::CustomContextMenu);
	connect(ui->tblCards, &qfw::TableView::customContextMenuRequested, this, &CardReaderWidget::onCustomContextMenuRequest);

	connect(ui->tblCards, &qfw::TableView::editCellRequest, this, [this](QModelIndex index) {
		auto col = index.column();
		if(col == Model::col_runFlags) {
			Runs::RunFlagsDialog dlg(this);
			dlg.load(m_cardsModel, ui->tblCards->toTableModelRowNo(ui->tblCards->currentIndex().row()));
			if(dlg.exec()) {
				dlg.save();
			}
		}
	}, Qt::QueuedConnection);
}

CardReaderWidget::~CardReaderWidget()
{
	delete ui;
}

void CardReaderWidget::onTestButtonClicked()
{
	const QByteArray data1 = QByteArray::fromHex(
	"02e18300010001010101edededed55aa"
	"000a6794f9950064090a0d0301c20c0f"
	"a2f00c03a25d0c01a21effffffff0000"
	"000020202020426f72696c2020202020"
	"20202020202020202020546f6d617320"
	"20202020202020202020202020202020"
	"20202020202020202020202020202020"
	"20202020202020202020202020202020"
	"20202020202036ae03");
	const QByteArray data2 = QByteArray::fromHex(
	"02e1830001060c6ba4210c6ca4800c6d"
	"a4d60c8aa57a0c7ea6710c7ba8840d78"
	"00710d7301001d64016aeeeeeeeeeeee"
	"eeeeeeeeeeeeeeeeeeeeeeeeeeeeeeee"
	"eeeeeeeeeeeeeeeeeeeeeeeeeeeeeeee"
	"eeeeeeeeeeeeeeeeeeeeeeeeeeeeeeee"
	"eeeeeeeeeeeeeeeeeeeeeeeeeeeeeeee"
	"eeeeeeeeeeeeeeeeeeeeeeeeeeeeeeee"
	"eeeeeeeeeeeeb7e403");
	const QByteArray data3 = QByteArray::fromHex(
	"02e183000107eeeeeeeeeeeeeeeeeeee"
	"eeeeeeeeeeeeeeeeeeeeeeeeeeeeeeee"
	"eeeeeeeeeeeeeeeeeeeeeeeeeeeeeeee"
	"eeeeeeeeeeeeeeeeeeeeeeeeeeeeeeee"
	"eeeeeeeeeeeeeeeeeeeeeeeeeeeeeeee"
	"eeeeeeeeeeeeeeeeeeeeeeeeeeeeeeee"
	"eeeeeeeeeeeeeeeeeeeeeeeeeeeeeeee"
	"eeeeeeeeeeeeeeeeeeeeeeeeeeeeeeee"
	"eeeeeeeeeeee07ac03");
	QList<QByteArray> lst{data1, data2, data3};
	Q_ASSERT(ui->btTest);
	static int count = -1;
	if(count < 0) {
		++count;
		ui->btTest->setText("Send packet #" + QString::number(count));
		return;
	}
	QByteArray ba = lst[count];
	logDriverRawData(ba);
	siDriver()->processData(ba);
	count++;
	count %= lst.size();
	ui->btTest->setText("Send packet #" + QString::number(count));
}

void CardReaderWidget::onCustomContextMenuRequest(const QPoint & pos)
{
	qfLogFuncFrame();
	QAction a_show_receipt(tr("Show receipt"), nullptr);
	QAction a_print_receipt(tr("Print receipt"), nullptr);
	QAction a_sep1(nullptr); a_sep1.setSeparator(true);
	QAction a_show_card(tr("Show card data"), nullptr);
	QAction a_print_card(tr("Print card data"), nullptr);
	QAction a_sep2(nullptr); a_sep2.setSeparator(true);
	QAction a_recalculate_times(tr("Recalculate times in selected rows"), nullptr);
	QList<QAction*> lst;
	lst << &a_show_receipt << &a_print_receipt
		<< &a_sep1
		<< &a_show_card << &a_print_card
		<< &a_sep2
		<< m_actAssignCard << &a_recalculate_times;
	QAction *a = QMenu::exec(lst, ui->tblCards->viewport()->mapToGlobal(pos));
	if(a == &a_show_receipt) {
		showSelectedReceipt();
	}
	if(a == &a_show_card) {
		showSelectedCard();
	}
	else if(a == &a_print_receipt) {
		int card_id = ui->tblCards->tableRow().value("cards.id").toInt();
		getPlugin<ReceiptsPlugin>()->printReceipt(card_id);
	}
	else if(a == &a_print_card) {
		int card_id = ui->tblCards->tableRow().value("cards.id").toInt();
		getPlugin<ReceiptsPlugin>()->printCard(card_id);
	}
	else if(a == &a_recalculate_times) {
		qf::gui::framework::MainWindow *fwk = qf::gui::framework::MainWindow::frameWork();
		int curr_ix = 0;
		QList<int> sel_ixs = ui->tblCards->selectedRowsIndexes();
		for(int ix : sel_ixs) {
			qf::core::utils::TableRow row = ui->tblCards->tableRow(ix);
			int card_id = row.value(QStringLiteral("id")).toInt();
			int run_id = row.value(QStringLiteral("runId")).toInt();
			fwk->showProgress(tr("Recalculating times for %1").arg(row.value(QStringLiteral("competitorName")).toString()), ++curr_ix, sel_ixs.count());
			try {
				qf::core::sql::Transaction transaction;
				getPlugin<CardReaderPlugin>()->reloadTimesFromCard(card_id, run_id, false);
				ui->tblCards->reloadRow(ix);
				transaction.commit();
			}
			catch (const qf::core::Exception &e) {
				qfError() << "Update runs & runlaps ERROR:" << e.message();
			}
		}
		fwk->hideProgress();
	}
}

void CardReaderWidget::settleDownInPartWidget(::PartWidget *part_widget)
{
	connect(part_widget, &::PartWidget::resetPartRequest, this, &CardReaderWidget::reset);
	connect(part_widget, &::PartWidget::reloadPartRequest, this, &CardReaderWidget::reload);
	{
		qfw::Action *a_station = part_widget->menuBar()->actionForPath("station", true);
		a_station->setText(tr("&Station"));
		//a_station->addActionInto(m_actSettings);
		//a_station->addActionInto(m_actCommOpen);
		{
			auto *a = new QAction(tr("Station info"));
			a->setEnabled(false);
			connect(commPort(), &siut::CommPort::openChanged, a, &QAction::setEnabled);
			connect(a, &QAction::triggered, this, [this]() {
				auto *cmd = new siut::SiTaskStationConfig();
				connect(cmd, &siut::SiTaskStationConfig::finished, this, [this](bool ok, QVariant result) {
					if(ok) {
						siut::SiStationConfig cfg(result.toMap());
						QString msg = cfg.toString();
						qf::gui::dialogs::MessageBox::showInfo(this, msg);
					}
				}, Qt::QueuedConnection);
				this->siDriver()->setSiTask(cmd);
			});
			a_station->addActionInto(a);
		}
		{
			auto *a = new QAction(tr("Read station memory"));
			a->setEnabled(false);
			connect(commPort(), &siut::CommPort::openChanged, a, &QAction::setEnabled);
			connect(a, &QAction::triggered, this, &CardReaderWidget::readStationBackupMemory);
			a_station->addActionInto(a);
		}
		/*
		{
			QAction *a = new QAction("Set master mode");
			connect(a, &QAction::triggered, [this]() {
				CmdSetDirectRemoteMode *cmd = new CmdSetDirectRemoteMode(true);
				this->siDriver()->enqueueSiCommand(cmd);
			});
			a_station->addActionInto(a);
		}
		{
			QAction *a = new QAction("Set slave mode");
			connect(a, &QAction::triggered, [this]() {
				CmdSetDirectRemoteMode *cmd = new CmdSetDirectRemoteMode(false);
				this->siDriver()->enqueueSiCommand(cmd);
			});
			a_station->addActionInto(a);
		}
		*/
	}
	{
		qfw::Action *a_tools = part_widget->menuBar()->actionForPath("tools", true);
		a_tools->setText(tr("&Tools"));
		{
			auto *m_import_cards = a_tools->addMenuInto("importCards", tr("Import cards"));
			{
				auto *a = new qfw::Action(tr("Laps only CSV"));
				connect(a, &qf::gui::Action::triggered, this, &CardReaderWidget::importCards_lapsOnlyCsv);
				m_import_cards->addActionInto(a);
			}
			{
				auto *a = new qfw::Action(tr("SI reader backup memory CSV"));
				connect(a, &qf::gui::Action::triggered, this, &CardReaderWidget::importCards_SIReaderBackupMemoryCsv);
				m_import_cards->addActionInto(a);
				m_import_cards->addActionInto(a);
			}
		}
		{
			auto *a = new qfw::Action(tr("Test audio"));
			connect(a, &qf::gui::Action::triggered, this, &CardReaderWidget::operatorAudioNotify);
			a_tools->addActionInto(a);
		}
	}
	connect(getPlugin<EventPlugin>(), &Event::EventPlugin::dbEventNotify, this, &CardReaderWidget::onDbEventNotify, Qt::QueuedConnection);
}

void CardReaderWidget::reset()
{
	if(!getPlugin<EventPlugin>()->isEventOpen()) {
		m_cardsModel->clearRows();
		return;
	}
	reload();
}

void CardReaderWidget::reload()
{
	bool is_relays = getPlugin<EventPlugin>()->eventConfig()->isRelays();
	//QString driver_name = m_cardsModel->sqlConnection().driverName();
	int current_stage = getPlugin<CardReaderPlugin>()->currentStageId();
	qfs::QueryBuilder qb;
	qb.select2("cards", "id, siId, runId, checkTime, startTime, finishTime, runIdAssignError")
			.select2("runs", "id, startTimeMs, timeMs, finishTimeMs, misPunch, disqualified, badCheck, notStart, notFinish, disqualifiedByOrganizer, overTime, notCompeting, cardLent, cardReturned")
			.select2("competitors", "registration, startNumber")
			.select2("classes", "name")
			.select("COALESCE(lastName, '') || ' ' || COALESCE(firstName, '') AS competitorName")
			.select("lentcards.siid IS NOT NULL AS cardLentTable")
			.select("'' AS runFlags")
			.from("cards")
			.joinRestricted("cards.siId", "lentcards.siid", "NOT lentcards.ignored")
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
	qfDebug() << qb.toString();
	m_cardsModel->setQueryBuilder(qb, false);
	m_cardsModel->reload();
}

void CardReaderWidget::onDbEventNotify(const QString &domain, int connection_id, const QVariant &data)
{
	Q_UNUSED(connection_id)
	if(domain == QLatin1String(Event::EventPlugin::DBEVENT_CARD_READ)) {
		int card_id = data.toInt();
		if(isVisible()) {
			updateTableView(card_id);
		}
	}
}

void CardReaderWidget::createActions()
{
	{
		auto *a = new qf::gui::Action(tr("Assign card to runner\tCtrl + Enter"), this);
		a->setShortcut(Qt::CTRL | Qt::Key_Return); // Qt::Key_Return is the main enter key, Qt::Key_Enter is on the numeric keyboard
		addAction(a);
		connect(a, &QAction::triggered, this, &CardReaderWidget::assignRunnerToSelectedCard);
		m_actAssignCard = a;
	}
}

siut::DeviceDriver *CardReaderWidget::siDriver()
{
	if(!f_siDriver) {
		f_siDriver = new siut::DeviceDriver(this);
		connect(commPort(), &siut::CommPort::readyRead, this, [this]() {
			QByteArray ba = commPort()->readAll();
			logDriverRawData(ba);
			siDriver()->processData(ba);
		});
		connect(f_siDriver, &siut::DeviceDriver::dataToSend, commPort(), &siut::CommPort::sendData);
		connect(f_siDriver, &siut::DeviceDriver::siTaskFinished, this, &CardReaderWidget::onSiTaskFinished);
		connect(f_siDriver, &siut::DeviceDriver::driverInfo, this, &CardReaderWidget::processDriverInfo, Qt::QueuedConnection);
	}
	return f_siDriver;
}

siut::CommPort *CardReaderWidget::commPort()
{
	if(!m_commPort) {
		m_commPort = new siut::CommPort(this);
		connect(m_commPort, &siut::CommPort::openChanged, this, &CardReaderWidget::onComOpenChanged);
	}
	return m_commPort;
}

void CardReaderWidget::onComOpenChanged(bool comm_is_open)
{
	if(comm_is_open) {
		auto *cmd = new siut::SiTaskSetDirectRemoteMode(siut::SiTaskSetDirectRemoteMode::Mode::Direct);
		connect(cmd, &siut::SiTaskSetDirectRemoteMode::finished, this, [this](bool ok) {
			if(ok) {
				ui->lblConnectionInfo->setText(tr("Connected to %1 in direct mode.").arg(this->commPort()->portName()));
			}
			else {
				ui->lblConnectionInfo->setText(tr("Error set SI station to direct mode."));
			}
		});
		siDriver()->setSiTask(cmd);
	}
	else {
		ui->lblConnectionInfo->setText(tr("SI station not connected"));
	}
}

void CardReaderWidget::onOpenCommTriggered(bool checked)
{
	qfLogFuncFrame() << "checked:" << checked;
	if(checked) {
		CardReaderSettings settings;
		QString device = settings.device();
		int baud_rate = settings.baudRate();
		int data_bits = settings.dataBits();
		int stop_bits = settings.stopBits();
		QString parity = settings.parity();
		if(!commPort()->openComm(device, baud_rate, data_bits, parity, stop_bits > 1)) {
			QString error_msg = commPort()->errorToUserHint();
			qf::gui::dialogs::MessageBox::showError(this, tr("Error open device %1 - %2").arg(device).arg(error_msg));
		}
		//theApp()->scriptDriver()->callExtensionFunction("onCommConnect", QVariantList() << device);
	}
	else {
		commPort()->closeComm();
	}
}

void CardReaderWidget::appendLog(NecroLog::Level level, const QString& msg)
{
	switch (level) {
	case NecroLog::Level::Debug: qfDebug() << msg; break;
	case NecroLog::Level::Info: qfInfo() << msg; break;
	case NecroLog::Level::Warning: qfWarning() << msg; break;
	default: qfError() << msg; break;
	}
}

void CardReaderWidget::onSiTaskFinished(int task_type, QVariant result)
{
	qfLogFuncFrame();
	auto tt = static_cast<siut::SiTask::Type>(task_type);
	if(tt == siut::SiTask::Type::CardRead) {
		siut::SICard card(result.toMap());
		if(card.isEmpty())
			qfError() << "Empty card received";
		else
			processSICard(card);
	}
	else if(tt == siut::SiTask::Type::Punch) {
		siut::SIPunch punch(result.toMap());
		if(punch.isEmpty())
			qfError() << "Empty punch received";
		else
			processSIPunch(punch);
	}
}

void CardReaderWidget::processDriverInfo(NecroLog::Level level, const QString& msg)
{
	CardReaderSettings settings;
	if(settings.isShowRawComData()) {
		if(level == NecroLog::Level::Debug)
			level = NecroLog::Level::Info;
	}
	appendLog(level, tr("DriverInfo: <%1> %2").arg(NecroLog::levelToString(level)).arg(msg));
}

void CardReaderWidget::logDriverRawData(const QByteArray& data)
{
	CardReaderSettings settings;
	if(settings.isShowRawComData()) {
		QString msg = siut::SIMessageData::dumpData(data, 16);
		appendLog(NecroLog::Level::Info, tr("DriverRawData: %1").arg(msg));
	}
}

void CardReaderWidget::processSICard(const siut::SICard &card)
{
	if(card.cardNumber() == 0) {
		qfWarning() << "SIID == 0 was read!";
		return;
	}
	appendLog(NecroLog::Level::Debug, card.toString());
	appendLog(NecroLog::Level::Info, tr("card: %1").arg(card.cardNumber()));

	if(currentReaderMode() == CardReaderSettings::ReaderMode::EditOnPunch) {
		getPlugin<RunsPlugin>()->editCompetitorOnPunch(card.cardNumber());
		return;
	}

	QString err_msg;
	int run_id = getPlugin<CardReaderPlugin>()->findRunId(card.cardNumber(), card.finishTime(), &err_msg);

	if(run_id == 0) {
		operatorAudioWakeUp();
		appendLog(NecroLog::Level::Error, err_msg);
	}
	else {
		bool card_lent = getPlugin<CardReaderPlugin>()->isCardLent(card.cardNumber(), card.finishTime(), run_id);
		if(card_lent)
			operatorAudioNotify();
	}
	quickevent::core::si::ReadCard read_card(card);
	read_card.setRunId(run_id);
	read_card.setRunIdAssignError(err_msg);
	processReadCardInTransaction(read_card);
}

bool CardReaderWidget::processReadCardInTransaction(const quickevent::core::si::ReadCard &read_card)
{
	try {
		qf::core::sql::Transaction transaction;
		processReadCard(read_card);
		transaction.commit();
		return true;
	}
	catch (qf::core::Exception &e) {
		qfError() << "ERROR processReadCardSafe:" << e.message();
	}
	return false;
}

void CardReaderWidget::processReadCard(const quickevent::core::si::ReadCard &read_card)
{
	int card_id = getPlugin<CardReaderPlugin>()->saveCardToSql(read_card);
	if(card_id && read_card.runId()) {
		getPlugin<CardReaderPlugin>()->assignCardToRun(card_id, read_card.runId());
	}
	if(card_id) {
		/// receipts printer needs this
		/// emitDbEvent is using queued invocation
		getPlugin<EventPlugin>()->emitDbEvent(Event::EventPlugin::DBEVENT_CARD_READ, card_id, true);
	}
}

void CardReaderWidget::processSIPunch(const siut::SIPunch &rec)
{
	quickevent::core::si::PunchRecord punch(rec);
	punch.setsiid(rec.cardNumber());
	if(currentReaderMode() == CardReaderSettings::ReaderMode::Readout) {
		int run_id = getPlugin<CardReaderPlugin>()->findRunId(rec.cardNumber(), siut::SICard::INVALID_SI_TIME);
		if(run_id == 0) {
			appendLog(NecroLog::Level::Error, tr("Cannot find run for punch record SI: %1").arg(rec.cardNumber()));
		} else {
			punch.setrunid(run_id);
		}
	}
	int punch_id = getPlugin<CardReaderPlugin>()->savePunchRecordToSql(punch);
	if(punch_id > 0) {
		appendLog(NecroLog::Level::Debug, tr("Saved punch: %1 %2").arg(rec.cardNumber()).arg(rec.code()));
		punch.setid(punch_id);
		getPlugin<EventPlugin>()->emitDbEvent(Event::EventPlugin::DBEVENT_PUNCH_RECEIVED, punch, true);
	}
}

void CardReaderWidget::updateTableView(int card_id)
{
	if(card_id <= 0)
		return;
	m_cardsModel->insertRow(0);
	m_cardsModel->setValue(0, QStringLiteral("cards.id"), card_id);
	int reloaded_row_cnt = m_cardsModel->reloadRow(0);
	if(reloaded_row_cnt != 1) {
		qfWarning() << "Inserted/Copied row id:" << card_id << "reloaded in" << reloaded_row_cnt << "instances.";
		return;
	}
	ui->tblCards->updateRow(0);
}

void CardReaderWidget::showSelectedReceipt()
{
	qfLogFuncFrame();
	int card_id = ui->tblCards->tableRow().value("cards.id").toInt();
	getPlugin<ReceiptsPlugin>()->previewReceipt(card_id);
}

void CardReaderWidget::showSelectedCard()
{
	qfLogFuncFrame();
	int card_id = ui->tblCards->selectedRow().value("cards.id").toInt();
	getPlugin<ReceiptsPlugin>()->previewCard(card_id);
}

void CardReaderWidget::assignRunnerToSelectedCard()
{
	qfLogFuncFrame();
	int card_id = ui->tblCards->tableRow().value("cards.id").toInt();
	QF_ASSERT(card_id > 0, "Bad card id!", return);
	auto *w = new Runs::FindRunnerWidget(getPlugin<EventPlugin>()->currentStageId());
	w->setWindowTitle(tr("Assign card to runner"));
	qfd::Dialog dlg(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
	dlg.setPersistentSettingsId("dlgAssignRunnerToSelectedCard");
	//dlg.setDefaultButton(QDialogButtonBox::Ok);
	dlg.setCentralWidget(w);
	//w->setFocusToWidget(Runs::FindRunnerWidget::FocusWidget::Name);
	w->focusLineEdit();
	if(dlg.exec()) {
		QVariantMap values = w->selectedRunner();
		//qfDebug() << values;
		int run_id = values.value("runid").toInt();
		int si_id = getPlugin<CardReaderPlugin>()->cardIdToSiId(card_id);
		if(run_id <= 0 || si_id <= 0) {
			return;
		}
		qf::core::sql::QueryBuilder qb;
		qb.select("stageId").select("isRunning").from("runs").where("id=" QF_IARG(run_id) );
		qf::core::sql::Query q;
		q.execThrow(qb.toString());
		if (!q.next()) {
			return;
		}
		int stage_id = q.value("stageId").toInt();
		int competitor_id = values.value("competitorid").toInt();
		{
			bool is_running = q.value("isRunning").toBool();
			if (!is_running) {
				int ret = QMessageBox::warning(this, tr("Competitor off-race"),
								tr("Runner to which you are assinging SI card\n"
								   "is currently flagged \"not running\" for this stage (race).\n"
								   "If you continue, this flag will be removed"),
								QMessageBox::Ok | QMessageBox::Cancel);
				if (ret == QMessageBox::Cancel)
					return;
				QString qs = "UPDATE runs SET isRunning=true WHERE competitorId=" QF_IARG(competitor_id) " AND stageId=" QF_IARG(stage_id);
				q.execThrow(qs);
			}
		}
		{
			getPlugin<CardReaderPlugin>()->assignCardToRun(card_id, run_id);
			QString qs = "UPDATE runs SET siId=" QF_IARG(si_id) " WHERE competitorId=" QF_IARG(competitor_id) " AND stageId=" QF_IARG(stage_id);
			if(values.value(Runs::FindRunnerWidget::UseSIInNextStages).toBool()) {
				qs = "UPDATE runs SET siId=" QF_IARG(si_id) " WHERE competitorId=" QF_IARG(competitor_id) " AND stageId>=" QF_IARG(stage_id);
			}
			q.execThrow(qs);
		}

		this->ui->tblCards->reloadRow();

		getPlugin<ReceiptsPlugin>()->printOnAutoPrintEnabled(card_id);
	}
}

quickevent::gui::audio::Player *CardReaderWidget::audioPlayer()
{
	if(!m_audioPlayer)
		m_audioPlayer = new quickevent::gui::audio::Player(this);
	return m_audioPlayer;
}

void CardReaderWidget::operatorAudioWakeUp()
{
	audioPlayer()->playAlert(quickevent::gui::audio::Player::AlertKind::OperatorWakeUp);
}

void CardReaderWidget::operatorAudioNotify()
{
	audioPlayer()->playAlert(quickevent::gui::audio::Player::AlertKind::OperatorNotify);
}

CardReaderSettings::ReaderMode CardReaderWidget::currentReaderMode() const
{
	CardReaderSettings s;
	return s.readerModeEnum();
}

static int msecToSISec(int msec)
{
	//static constexpr int secs_to_noon = 12 * 60 * 60;
	return (msec / 1000);// % secs_to_noon;
}

static int obStringTosec(const QString &time_str)
{
	bool ok;
	int min = time_str.section('.', 0, 0).toInt(&ok);
	int sec = 0;
	QF_ASSERT_EX(ok == true, QString("Cannot convert time string '%1'!").arg(time_str));
	QString sec_str = time_str.section('.', 1).trimmed();
	if(!sec_str.isEmpty()) {
		if(sec_str.length() == 1)
			sec_str = sec_str + '0';
		sec = sec_str.toInt(&ok);
		QF_ASSERT_EX(ok == true, QString("Cannot convert time string '%1'!").arg(time_str));
	}
	return (60 * min + sec) * 1000;
}

static QList<int> codesForClassName(const QString &class_name, int stage_id)
{
	QList<int> ret;
	int course_id = 0;
	{
		qfs::QueryBuilder qb;
		qb.select2("classdefs", "courseId")
				.from("classes")
				.joinRestricted("classes.id", "classdefs.classId", "classdefs.stageId=" QF_IARG(stage_id))
				.where("classes.name=" QF_SARG(class_name));
		qfs::Query q;
		q.exec(qb.toString(), qf::core::Exception::Throw);
		if(q.next())
			course_id = q.value(0).toInt();
	}
	QF_ASSERT_EX(course_id > 0, QString("Cannot find course for class %1 and stage %2").arg(class_name).arg(stage_id));
	{
		qfs::QueryBuilder qb;
		qb.select2("coursecodes", "position")
				.select2("codes", "code")
				.from("coursecodes")
				.join("coursecodes.codeId", "codes.id")
				.where("coursecodes.courseId=" QF_IARG(course_id))
				.orderBy("coursecodes.position");
		qfs::Query q;
		q.exec(qb.toString(), qf::core::Exception::Throw);
		while (q.next()) {
			int code = q.value("code").toInt();
			QF_ASSERT_EX(code > 0, "Code must be > 0");
			ret << code;
		}
	}
	QF_ASSERT_EX(ret.count() > 0, QString("Cannot load codes for class %1 and stage %2").arg(class_name).arg(stage_id));
	return ret;
}

void CardReaderWidget::importCards_lapsOnlyCsv()
{
	// CSV record must have format:
	// 7203463,"2,28","3,34","2,42","3,29","3,12","1,38","1,13","3,18","1,17","0,15"
	// CSV rows can be commented by #
	qfLogFuncFrame();
	qf::gui::dialogs::MessageBox::showInfo(this, tr("<p>CSV record must have format:</p>"
														   "<p>7203463,\"2,28\",\"3,34\",\"2,42\",\"3,29\",\"3,12\",\"1,38\",\"1,13\",\"3,18\",\"1,17\",\"0,15\"</p>"
														   "<p>Any row can be commented by leading #</p>"
														   "<p>Decimal point is also supported, the quotes can be omited than.</p>"));
	QString fn = qf::gui::dialogs::FileDialog::getOpenFileName(this, tr("Import CSV"));
	if(fn.isEmpty())
		return;
	QFile f(fn);
	if(!f.open(QFile::ReadOnly)) {
		qf::gui::dialogs::MessageBox::showError(this, tr("Cannot open file '%1' for reading.").arg(f.fileName()));
		return;
	}
	QTextStream ts(&f);
	qf::core::utils::CSVReader reader(&ts);
	//reader.setSeparator(';');
	reader.setLineComment('#');
	try {
		qf::core::sql::Transaction transaction;
		while (!ts.atEnd()) {
			QStringList sl = reader.readCSVLineSplitted();
			// remove empty strings in the end of line
			int csv_ix = sl.indexOf(QString());
			if(csv_ix >= 0)
				sl = sl.mid(0, csv_ix);
			csv_ix = 0;
			int si_id = sl.value(csv_ix++).toInt();
			QF_ASSERT_EX(si_id > 0, "Bad SI!");
			int stage_id = getPlugin<EventPlugin>()->currentStageId();
			QF_ASSERT_EX(stage_id > 0, tr("Bad stage!"));
			int start00 = getPlugin<EventPlugin>()->stageStartMsec(stage_id);
			int run_id = 0;
			int start_time = 0;
			QString class_name;
			{
				qfs::QueryBuilder qb;
				qb.select2("runs", "id, startTimeMs")
						.select2("classes", "name")
						.from("runs")
						.innerJoin("runs.competitorId", "competitors.id")
						.innerJoin("competitors.classId", "classes.id")
						.where("runs.stageId=" QF_IARG(stage_id))
						.where("runs.siId=" QF_IARG(si_id)) ;
				qfs::Query q;
				q.exec(qb.toString(), qf::core::Exception::Throw);
				if(q.next()) {
					run_id = q.value("runs.id").toInt();
					start_time = start00 + q.value("runs.startTimeMs").toInt();
					class_name = q.value("classes.name").toString();
				}
			}
			QF_ASSERT_EX(run_id > 0, tr("Cannot find runs record for SI %1!").arg(si_id));
			QF_ASSERT_EX(!class_name.isEmpty(), tr("Cannot find class for SI %1!").arg(si_id));
			quickevent::core::si::ReadCard read_card;
			read_card.setCardNumber(si_id);
			read_card.setRunId(run_id);
			read_card.setStartTime(msecToSISec(start_time));
			read_card.setCheckTime(msecToSISec(start_time - (1000 * 90)));
			QVariantList punches;
			int stp_time = start_time;
			QList<int> codes = codesForClassName(class_name, stage_id);
			codes << quickevent::core::CodeDef::FINISH_PUNCH_CODE;
			if(csv_ix + codes.count() != sl.count()) {
				//qfWarning() << codes;
				qfWarning() << sl;
				QF_EXCEPTION(tr("SI: %1 class %2 - Number of punches (%3) and number of codes including finish (%4) should be the same! Remove or comment invalid line by #.")
							 .arg(si_id).arg(class_name).arg(sl.count() - csv_ix).arg(codes.count()));
			}
			for (int i = 0; i < codes.count(); ++i) {
				QString lap_str = sl.value(csv_ix++);
				lap_str.replace(',', '.');
				int lap_time = obStringTosec(lap_str);
				stp_time += lap_time;
				quickevent::core::si::ReadPunch punch;
				punch.setCode(codes[i]);
				punch.setTime(msecToSISec(stp_time));
				punches << punch;
			}
			read_card.setFinishTime(quickevent::core::si::ReadPunch(punches.takeLast().toMap()).time());
			read_card.setPunches(punches);
			qfDebug() << read_card.toString();
			processReadCard(read_card);
		}
		transaction.commit();
	}
	catch (const qf::core::Exception &e) {
		qf::gui::dialogs::MessageBox::showException(this, e);
	}
}

void CardReaderWidget::importCards_SIReaderBackupMemoryCsv()
{
	qfLogFuncFrame();
	// No
	// Read on
	// SIID
	// Start no
	// Clear CN
	// Clear DOW
	// Clear time
	// Check CN
	// Check DOW
	// Check time
	// Start CN
	// Start DOW
	// Start time
	// Finish CN
	// Finish DOW
	// Finish time
	// Class
	// First name
	// Last name
	// Club
	// Country
	// Email
	// Date of birth
	// Sex
	// Phone
	// Street
	// ZIP
	// City
	// Hardware version
	// Software version
	// Battery date
	// Battery voltage
	// Clear count
	// Character set
	// SEL_FEEDBACK
	// No. of records
	// Record 1 CN
	// Record 1 DOW
	// Record 1 time
	// Record 2 CN
	// Record 2 DOW
	// Record 2 time
	// Record 3 CN
	// Record 3 DOW
	// Record 3 time
	// Record 4 CN
	// Record 4 DOW
	// Record 4 time
	// ...
	// Record 70 CN
	// Record 70 DOW
	// Record 70 time
	// Record 71 CN
	enum {
		col_No = 0,
		col_Read_on,
		col_SIID,
		col_Start_no,
		col_Clear_CN,
		col_Clear_DOW,
		col_Clear_time,
		col_Check_CN,
		col_Check_DOW,
		col_Check_time,
		col_Start_CN,
		col_Start_DOW,
		col_Start_time,
		col_Finish_CN,
		col_Finish_DOW,
		col_Finish_time,
		col_Class,
		col_First_name,
		col_Last_name,
		col_Club,
		col_Country,
		col_Email,
		col_Date_of_birth,
		col_Sex,
		col_Phone,
		col_Street,
		col_ZIP,
		col_City,
		col_Hardware_version,
		col_Software_version,
		col_Battery_date,
		col_Battery_voltage,
		col_Clear_count,
		col_Character_set,
		col_SEL_FEEDBACK,
		col_No_of_records,
		col_Record_1_CN,
		col_Record_1_DOW,
		col_Record_1_time,
	};
	//qf::gui::dialogs::MessageBox::showInfo(this, tr("<p>CSV record must have format:</p>"
	//													   "<p>7203463,\"2,28\",\"3,34\",\"2,42\",\"3,29\",\"3,12\",\"1,38\",\"1,13\",\"3,18\",\"1,17\",\"0,15\"</p>"
	//													   "<p>Any row can be commented by leading #</p>"
	//													   "<p>Decimal point is also supported, the quotes can be omited than.</p>"));
	QString fn = qf::gui::dialogs::FileDialog::getOpenFileName(this, tr("Import TXT"));
	if(fn.isEmpty())
		return;
	QFile f(fn);
	if(!f.open(QFile::ReadOnly)) {
		qf::gui::dialogs::MessageBox::showError(this, tr("Cannot open file '%1' for reading.").arg(f.fileName()));
		return;
	}
	int stage_id = getPlugin<EventPlugin>()->currentStageId();
	QF_ASSERT_EX(stage_id > 0, tr("Bad stage!"));
	int start00 = getPlugin<EventPlugin>()->stageStartMsec(stage_id);

	QTextStream ts(&f);
	qf::core::utils::CSVReader reader(&ts);
	reader.setSeparator(';');
	reader.setLineComment('#');
	try {
		qf::core::sql::Transaction transaction;
		int line_no = 0;
		while (!ts.atEnd()) {
			QStringList sl = reader.readCSVLineSplitted();
			if(line_no++ == 0)
				continue; /// skip hading
			int si_id = sl.value(col_SIID).toInt();
			if(si_id == 0) {
				qfError() << "Ignoring bad SI:" << sl.value(col_SIID);
				continue;
			}
			int run_id = 0;
			int start_time = 0;
			{
				qfs::QueryBuilder qb;
				qb.select2("runs", "id, startTimeMs")
						.select2("classes", "name")
						.from("runs")
						.innerJoin("runs.competitorId", "competitors.id")
						.innerJoin("competitors.classId", "classes.id")
						.where("runs.stageId=" QF_IARG(stage_id))
						.where("runs.siId=" QF_IARG(si_id)) ;
				qfs::Query q;
				q.exec(qb.toString(), qf::core::Exception::Throw);
				if(q.next()) {
					run_id = q.value("runs.id").toInt();
					start_time = start00 + q.value("runs.startTimeMs").toInt();
					//class_name = q.value("classes.name").toString();
				}
			}
			if(run_id == 0) {
				qfError() << "Cannot find runs record for SI:" << si_id;
				continue;
			}
			quickevent::core::si::ReadCard read_card;
			read_card.setCardNumber(si_id);
			read_card.setRunId(run_id);
			auto time_to_msec = [](const QString &s, int default_msec) {
				QTime tm = QTime::fromString(s, Qt::ISODate);
				return tm.isValid()? tm.msecsSinceStartOfDay(): default_msec;
			};
			read_card.setStartTime(msecToSISec(time_to_msec(sl.value(col_Start_time), start_time)));
			read_card.setCheckTime(msecToSISec(time_to_msec(sl.value(col_Check_time), start_time - (1000 * 90))));
			read_card.setFinishTime(msecToSISec(time_to_msec(sl.value(col_Finish_time), 0)));
			QVariantList punches;
			int rec_cnt = sl.value(col_No_of_records).toInt();
			for (int i = 0; i < rec_cnt; ++i) {
				int code = sl.value(col_Record_1_CN + (3*i)).toInt();
				int tm = msecToSISec(time_to_msec(sl.value(col_Record_1_time + (3*i)), 0));
				quickevent::core::si::ReadPunch punch;
				punch.setCode(code);
				punch.setTime(tm);
				punches << punch;
			}
			read_card.setFinishTime(quickevent::core::si::ReadPunch(punches.takeLast().toMap()).time());
			read_card.setPunches(punches);
			qfDebug() << read_card.toString();
			processReadCard(read_card);
		}
		transaction.commit();
	}
	catch (const qf::core::Exception &e) {
		qf::gui::dialogs::MessageBox::showException(this, e);
	}
}

void CardReaderWidget::readStationBackupMemory()
{
	auto *si_task = new siut::SiTaskReadStationBackupMemory();
	connect(si_task, &siut::SiTaskStationConfig::progress, this, [this, si_task](int phase, int count) {
		auto *progress_dlg = this->findChild<QProgressDialog*>(QString(), Qt::FindDirectChildrenOnly);
		if(progress_dlg == nullptr) {
			progress_dlg = new QProgressDialog(tr("Downloading station backup ..."), "Abort", 0, count, this);
			connect(progress_dlg, &QProgressDialog::canceled, si_task, [si_task]() {
				si_task->abortWithMessage(tr("Cancelled by user"));
			});
			connect(si_task, &siut::SiTask::finished, progress_dlg, [progress_dlg]() {
				progress_dlg->setParent(nullptr);
				progress_dlg->deleteLater();
			});
		}
		progress_dlg->setValue(phase);
	});
	connect(si_task, &siut::SiTaskStationConfig::finished, this, [this](bool ok, QVariant result) {
		if(ok) {
			//qf::gui::dialogs::MessageBox::showInfo(this, "memory read");
			QVariantMap m = result.toMap();
			int station_number = m.value("stationNumber").toInt();
			QVariantList punches = m.value("punches").toList();

			QVariantList html_rows;
			for (int i = 0; i < punches.size(); i++) {
				QVariantList punch = punches[i].toList();
				QVariantList tr{QStringLiteral("tr")};
				tr.insert(tr.length(), QVariantList{"td", QString::number(i+1)});

				int si = punch.value(0).toInt();
				tr.insert(tr.length(), QVariantList{"td", QString::number(si)});
				QDateTime punch_time = punch.value(1).toDateTime();
				tr.insert(tr.length(), QVariantList{"td", punch_time.toString(Qt::ISODateWithMs)});
				bool card_error = punch.value(2).toBool();
				tr.insert(tr.length(), QVariantList{"td", card_error? "Y": ""});
				html_rows.insert(html_rows.length(), tr);
			}
			const QStringList html_fields{tr("No."), tr("SI"), tr("DateTime"), tr("Card error")};
			QVariantList html_body = QVariantList() << QStringLiteral("body");
			html_body.insert(html_body.length(), QVariantList() << QStringLiteral("body"));
			html_body.insert(html_body.length(), qf::core::utils::HtmlUtils::createHtmlTable(tr("Station %1 backup memory").arg(station_number), html_fields, html_rows));
			qf::core::utils::HtmlUtils::FromHtmlListOptions opts;
			opts.setDocumentTitle(tr("Station backup memory"));
			QString html = qf::core::utils::HtmlUtils::fromHtmlList(html_body, opts);

			qf::gui::dialogs::Dialog dlg(QDialogButtonBox::Save | QDialogButtonBox::Cancel, this);
			/*
			qf::gui::DialogButtonBox *bbx = dlg.buttonBox();
			QPushButton *bt_set_off_race = new QPushButton(tr("Set off-race"));
			bt_set_off_race->setToolTip(tr("All runners without CHECK will be marked as off-race."));
			bool set_off_race = false;
			connect(bt_set_off_race, &QPushButton::clicked, [&set_off_race]() {
				set_off_race = true;
			});
			bbx->addButton(bt_set_off_race, QDialogButtonBox::AcceptRole);
			*/
			auto *w = new qf::gui::HtmlViewWidget();
			dlg.setCentralWidget(w);
			w->setHtmlText(html);
			if(dlg.exec()) {
				// save checks
				try {
					qfs::Transaction transaction;
					int stage_id = getPlugin<EventPlugin>()->currentStageId();
					qfs::Query q1;
					q1.prepare("INSERT INTO stationsbackup (stageId, stationNumber, siId, punchDateTime, cardErr) VALUES"
							  " (:stageId, :stationNumber, :siId, :punchDateTime, :cardErr)"
							  , qfc::Exception::Throw);
					QDateTime stage_start_dt = getPlugin<EventPlugin>()->stageStartDateTime(stage_id);
					qfs::Query q2;
					q2.prepare("UPDATE runs SET checkTimeMs=:checkTimeMs"
							  " WHERE siId=:siId AND checkTimeMs IS NULL AND stageId=" QF_IARG(stage_id)
							  , qfc::Exception::Throw);
					for (const auto & plist : punches) {
						QVariantList punch = plist.toList();
						q1.bindValue(QStringLiteral(":stageId"), stage_id);
						q1.bindValue(QStringLiteral(":stationNumber"), station_number);
						int si = punch.value(0).toInt();
						q1.bindValue(QStringLiteral(":siId"), si);
						QDateTime punch_time = punch.value(1).toDateTime();
						int check_time_msec = (int)stage_start_dt.msecsTo(punch_time);
						q1.bindValue(QStringLiteral(":punchDateTime"), punch_time);
						bool card_error = punch.value(2).toBool();

						q1.bindValue(QStringLiteral(":cardErr"), card_error);
						q1.exec(!qfc::Exception::Throw); // ignore unique key error

						q2.bindValue(QStringLiteral(":siId"), si);
						q2.bindValue(QStringLiteral(":checkTimeMs"), check_time_msec);
						q2.exec(qfc::Exception::Throw);
					}
					transaction.commit();
				}
				catch (qfc::Exception &ex) {
					qfd::MessageBox::showException(this, ex);
				}
			}
		}
		else {
			qfd::MessageBox::showError(this, result.toString());
		}
	}, Qt::QueuedConnection);
	siDriver()->setSiTask(si_task);
}

#include "cardreaderwidget.moc"

