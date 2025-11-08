#include "eventplugin.h"
#include "connectdbdialogwidget.h"
#include "connectionsettings.h"
#include "eventdialogwidget.h"
#include "dbschema.h"
#include "registrationswidget.h"
#include "lentcardssettingspage.h"
#include "stagewidget.h"
#include "stagedocument.h"
#include "../../Core/src/widgets/appstatusbar.h"

#include "services/serviceswidget.h"
#include "services/emmaclient.h"
#include "services/qx/qxeventservice.h"

#include <plugins/Core/src/widgets/settingsdialog.h>
#include <plugins/Event/src/services/oresultsclient.h>
#include <plugins/Core/src/coreplugin.h>

#include <quickevent/core/og/timems.h>

#include <qf/gui/framework/dockwidget.h>
#include <qf/gui/framework/mainwindow.h>
#include <qf/gui/framework/application.h>
#include <qf/gui/dialogs/dialog.h>
#include <qf/gui/dialogs/messagebox.h>
#include <qf/gui/dialogs/filedialog.h>
#include <qf/gui/action.h>
#include <qf/gui/menubar.h>
#include <qf/gui/statusbar.h>
#include <qf/gui/toolbar.h>
#include <qf/gui/style.h>

#include <qf/core/log.h>
#include <qf/core/assert.h>
#include <qf/core/sql/query.h>
#include <qf/core/sql/querybuilder.h>
#include <qf/core/sql/connection.h>
#include <qf/core/sql/transaction.h>
#include <qf/core/utils/fileutils.h>
#include <plugins/Event/src/services/ofeed/ofeedclient.h>

#include <QInputDialog>
#include <QSqlDatabase>
#include <QSqlRecord>
#include <QSqlField>
#include <QSqlError>
#include <QComboBox>
#include <QLabel>
#include <QMetaObject>
#include <QSqlDriver>
#include <QJsonObject>
#include <QPushButton>
#include <QToolButton>
#include <QDirIterator>
#include <QJsonParseError>
#include <QJsonDocument>
#include <QTimer>
#include <QRandomGenerator>

#include <regex>

namespace qfw = qf::gui;
namespace qff = qf::gui::framework;
namespace qfd = qf::gui::dialogs;
namespace qfs = qf::core::sql;

using qff::getPlugin;

namespace Event {

class DbEventPayload : public QVariantMap
{
private:
	typedef QVariantMap Super;

	QF_VARIANTMAP_FIELD(QString, e, setE, ventName)
	QF_VARIANTMAP_FIELD(QString, d, setD, omain)
	QF_VARIANTMAP_FIELD(int, c, setc, onnectionId)
	QF_VARIANTMAP_FIELD(QVariant, d, setD, ata)
public:
	DbEventPayload(const QVariantMap &data = QVariantMap()) : QVariantMap(data) {}

	static DbEventPayload fromJson(const QByteArray &json);

	QByteArray toJson() const;
};

DbEventPayload DbEventPayload::fromJson(const QByteArray &json)
{
	QJsonParseError error;
	QJsonDocument jsd = QJsonDocument::fromJson(json, &error);
	if(error.error == QJsonParseError::NoError) {
		QVariantMap m = jsd.toVariant().toMap();
		return DbEventPayload(m);
	}
	qfError() << "JSON parse error:" << error.errorString();
	return DbEventPayload();
}

QByteArray DbEventPayload::toJson() const
{
	QJsonDocument jsd = QJsonDocument::fromVariant(*this);
	return jsd.toJson(QJsonDocument::Compact);
}

namespace {
const auto QBE_EXT = QStringLiteral(".qbe");

QString singleFileStorageDir()
{
	ConnectionSettings connection_settings;
	QString ret = connection_settings.singleWorkingDir();
	return ret;
}

QString eventNameToFileName(const QString &event_name)
{
	QString ret = singleFileStorageDir() + '/' + event_name + QBE_EXT;
	return ret;
}

QString fileNameToEventName(const QString &file_name)
{
	QString fn = file_name;
	fn.replace("\\", "/");
	int ix = fn.lastIndexOf("/");
	QString event_name = fn.mid(ix + 1);
	if(event_name.endsWith(QBE_EXT, Qt::CaseInsensitive))
		event_name = event_name.mid(0, event_name.length() - QBE_EXT.length());
	return event_name;
}
}

EventPlugin::EventPlugin(QObject *parent)
	: Super("Event", parent)
{
	connect(this, &EventPlugin::installed, this, &EventPlugin::onInstalled);//, Qt::QueuedConnection);
	connect(this, &EventPlugin::currentStageIdChanged, this, &EventPlugin::saveCurrentStageId);
	connect(this, &EventPlugin::eventNameChanged, [this](const QString &event_name) {
		setEventOpen(!event_name.isEmpty());
	});
	connect(this, &Event::EventPlugin::dbEventNotify, this, &Event::EventPlugin::onDbEventNotify, Qt::QueuedConnection);
}

void EventPlugin::initEventConfig()
{
	if(m_eventConfig == nullptr) {
		m_eventConfig = new Event::EventConfig(this);
	}
	else {
		qfWarning() << "Event config exists already!";
	}
}

Event::EventConfig *EventPlugin::eventConfig(bool reload)
{
	if(m_eventConfig == nullptr) {
		m_eventConfig = new Event::EventConfig(this);
		reload = true;
	}
	if(reload) {
		m_eventConfig->load();
		//emit eventNameChanged(m_eventConfig->eventName());
	}
	return m_eventConfig;
}

int EventPlugin::stageCount()
{
	if(eventName().isEmpty())
		return 0;
	return eventConfig()->stageCount();
}

void EventPlugin::setCurrentStageId(int stage_id)
{
	if(m_currentStageId == stage_id) {
		return;
	}
	m_currentStageId = stage_id;
	emit currentStageIdChanged(stage_id);
}

int EventPlugin::currentStageId() const
{
	return m_currentStageId;
}

int EventPlugin::stageIdForRun(int run_id)
{
	int ret = 0;
	qfs::QueryBuilder qb;
	qb.select2("runs", "stageId")
			.from("runs")
			.where("runs.id=" QF_IARG(run_id));
	qfs::Query q;
	q.exec(qb.toString(), qf::core::Exception::Throw);
	if(q.next())
		ret = q.value(0).toInt();
	else
		qfError() << "Cannot find runs record for id:" << run_id;
	return ret;
}

int EventPlugin::stageStartMsec(int stage_id)
{
	QTime start_time = stageStartTime(stage_id);
	int ret = start_time.msecsSinceStartOfDay();
	return ret;
}

QDate EventPlugin::stageStartDate(int stage_id)
{
	return stageStartDateTime(stage_id).date();
}

QTime EventPlugin::stageStartTime(int stage_id)
{
	return stageStartDateTime(stage_id).time();
}

QDateTime EventPlugin::stageStartDateTime(int stage_id)
{
	Event::StageData stage_data = stageData(stage_id);
	QDateTime dt = stage_data.startDateTime();
	return dt;
}

int EventPlugin::msecToStageStartAM(int si_am_time_sec, int msec, int stage_id)
{
	if(si_am_time_sec == 0xEEEE)
		return quickevent::core::og::TimeMs::UNREAL_TIME_MSEC;
	if(stage_id == 0)
		stage_id = currentStageId();
	int stage_start_msec = stageStartMsec(stage_id);
	int time_msec = quickevent::core::og::TimeMs::msecIntervalAM(stage_start_msec, (si_am_time_sec * 1000) + msec);
	return time_msec;
}

StageData EventPlugin::stageData(int stage_id)
{
	if(!m_stageCache.contains(stage_id)) {
		StageDocument doc;
		doc.load(stage_id);
		StageData data;
		for (const auto &[k, v] : doc.values().asKeyValueRange()) {
			auto key = k.toLower();
			if (key == "drawingconfig" && v.userType() == qMetaTypeId<QString>()) {
				// convert json to variantmap
				data[key] = qf::core::Utils::jsonToQVariant(v.toString());
			}
			else {
				data[key] = v;
			}
		}
		m_stageCache[stage_id] = StageData(data);
	}
	return m_stageCache.value(stage_id);
}

void EventPlugin::setStageData(int stage_id, const StageData &data)
{
	StageDocument doc;
	doc.load(stage_id);
	for (const auto &[k, v] : data.asKeyValueRange()) {
		doc.setValue(k, v);
		m_stageCache[stage_id][k.toLower()] = v;
	}
	doc.save();
}

void EventPlugin::clearStageDataCache()
{
	qfInfo() << "stages data cache cleared";
	m_stageCache.clear();
}

void EventPlugin::onInstalled()
{
	qff::MainWindow *fwk = qff::MainWindow::frameWork();

	m_actConnectDb = new qfw::Action(tr("&Connect to database"));
	//a->setShortcut("ctrl+L");
	connect(m_actConnectDb, &QAction::triggered, this, &EventPlugin::connectToSqlServer);

	m_actOpenEvent = new qfw::Action(tr("&Open event"));
	//m_actOpenEvent->setShortcut("Ctrl+O");
	m_actOpenEvent->setEnabled(false);
	connect(m_actOpenEvent, &QAction::triggered, this, [this]() { openEvent(); });

	m_actCreateEvent = new qfw::Action(tr("Create eve&nt"));
	//m_actCreateEvent->setShortcut("Ctrl+N");
	connect(m_actCreateEvent, &QAction::triggered, this, [this]() { createEvent(); });

	m_actEditEvent = new qfw::Action(tr("E&dit event"));
	m_actEditEvent->setEnabled(false);
	connect(m_actEditEvent, &QAction::triggered, this, &EventPlugin::editEvent);
	connect(this, &EventPlugin::eventNameChanged, [this](const QString &event_name) {
		this->m_actEditEvent->setEnabled(!event_name.isEmpty());
	});

	m_actExportEvent_qbe = new qfw::Action(tr("Event (*.qbe)"));
	m_actExportEvent_qbe->setEnabled(false);
	connect(m_actExportEvent_qbe, &QAction::triggered, this, &EventPlugin::exportEvent_qbe);

	m_actImportEvent_qbe = new qfw::Action(tr("Event (*.qbe)"));
	connect(m_actImportEvent_qbe, &QAction::triggered, this, &EventPlugin::importEvent_qbe);

	if(auto *sb = qobject_cast<Core::AppStatusBar*>(fwk->statusBar())) {
		connect(this, &EventPlugin::eventNameChanged, sb, &Core::AppStatusBar::setEventName);
		connect(this, &EventPlugin::currentStageIdChanged, sb, &Core::AppStatusBar::setStageNo);
	}
	connect(this, &EventPlugin::eventNameChanged, this, &EventPlugin::updateWindowTitle);
	connect(this, &EventPlugin::currentStageIdChanged, this, &EventPlugin::updateWindowTitle);
	connect(fwk, &qff::MainWindow::applicationLaunched, this, &EventPlugin::connectToSqlServer);
	connect(this, &EventPlugin::eventOpenChanged, this, &EventPlugin::onEventOpened);

	qfw::Action *a_import = fwk->menuBar()->actionForPath("file/import", false);
	Q_ASSERT(a_import);
	a_import->addActionBefore(m_actConnectDb);
	a_import->addSeparatorBefore();

	m_actEvent = m_actConnectDb->addMenuAfter("file.event", tr("&Event"));
	m_actEvent->setEnabled(false);

	m_actEvent->addActionInto(m_actCreateEvent);
	m_actEvent->addActionInto(m_actOpenEvent);
	m_actEvent->addActionInto(m_actEditEvent);

	m_actImport = fwk->menuBar()->actionForPath("file/import");
	m_actImport->addActionInto(m_actImportEvent_qbe);
	m_actImport->setEnabled(false);

	m_actExport = fwk->menuBar()->actionForPath("file/export");
	m_actExport->addActionInto(m_actExportEvent_qbe);
	m_actExport->setEnabled(false);

	qfw::ToolBar *tb = fwk->toolBar("Event", true);
	tb->setObjectName("EventToolbar");
	tb->setWindowTitle(tr("Event"));
	{
		auto *bt_stage = new QToolButton();
		//bt_stage->setFlat(true);
		bt_stage->setAutoRaise(true);
		bt_stage->setCheckable(true);
		tb->addWidget(bt_stage);
		m_cbxStage = new QComboBox();
		connect(m_cbxStage, &QComboBox::activated, this, [this](int ix) {
			setCurrentStageId(ix + 1);
		});
		connect(this, &EventPlugin::currentStageIdChanged, bt_stage, [this, bt_stage](int stage_id) {
			QSignalBlocker b(m_cbxStage);
			m_cbxStage->setCurrentIndex(stage_id - 1);
			bt_stage->setText(tr("Current stage E%1").arg(stage_id));
		});
		QAction *act_stage = tb->addWidget(m_cbxStage);
		act_stage->setVisible(false);


		QIcon ico(qf::gui::Style::icon("settings"));
		m_actEditStage = new qfw::Action(ico, "Stage settings");
		m_actEditStage->setVisible(false);
		connect(m_actEditStage, &QAction::triggered, this, &EventPlugin::editStage);
		tb->addAction(m_actEditStage);

		connect(bt_stage, &QPushButton::clicked, this, [this, act_stage](bool checked) {
			act_stage->setVisible(checked);
			m_actEditStage->setVisible(checked);
		});
	}
	fwk->menuBar()->actionForPath("view/toolbar")->addActionInto(tb->toggleViewAction());

	auto *oresults_client = new services::OResultsClient(this);
	services::Service::addService(oresults_client);

	auto *ofeed_client = new services::OFeedClient(this);
	services::Service::addService(ofeed_client);

	auto *emma_client = new services::EmmaClient(this);
	services::Service::addService(emma_client);

	auto shvapi_client = new services::qx::QxEventService(this);
	services::Service::addService(shvapi_client);

	{
		m_servicesDockWidget = new qff::DockWidget(nullptr);
		m_servicesDockWidget->setObjectName("servicesDockWidget");
		m_servicesDockWidget->setWindowTitle(tr("Services"));
		m_servicesDockWidget->setMinimumWidth(230);
		fwk->addDockWidget(Qt::RightDockWidgetArea, m_servicesDockWidget);
		m_servicesDockWidget->hide();
		connect(m_servicesDockWidget, &qff::DockWidget::visibilityChanged, this, &EventPlugin::onServiceDockVisibleChanged);

		auto *a = m_servicesDockWidget->toggleViewAction();
		//a->setCheckable(true);
		//a->setShortcut(QKeySequence("ctrl+shift+R"));
		fwk->menuBar()->actionForPath("view")->addActionInto(a);
	}
	{
		connect(this, &Event::EventPlugin::eventOpenChanged, this, &EventPlugin::reloadRegistrationsModel);

		{
			m_registrationsDockWidget = new qff::DockWidget(nullptr);
			m_registrationsDockWidget->setObjectName("registrationsDockWidget");
			m_registrationsDockWidget->setWindowTitle(tr("Registrations"));
			fwk->addDockWidget(Qt::RightDockWidgetArea, m_registrationsDockWidget);
			m_registrationsDockWidget->hide();
			connect(m_registrationsDockWidget, &qff::DockWidget::visibilityChanged, this, &EventPlugin::onRegistrationsDockVisibleChanged);

			auto *a = m_registrationsDockWidget->toggleViewAction();
			//a->setCheckable(true);
			a->setShortcut(QKeySequence("ctrl+shift+R"));
			fwk->menuBar()->actionForPath("view")->addActionInto(a);
		}
		auto core_plugin = qf::gui::framework::getPlugin<Core::CorePlugin>();
		core_plugin->settingsDialog()->addPage(new LentCardsSettingsPage());
	}
}

void EventPlugin::updateWindowTitle() const
{
	QString title = QStringLiteral("%1 E%2").arg(eventName()).arg(currentStageId());
	qff::MainWindow *fwk = qff::MainWindow::frameWork();
	fwk->setWindowTitle(title);
}

void EventPlugin::loadCurrentStageId()
{
	int stage_id = 1;
	if(!eventName().isEmpty()) {
		stage_id = eventConfig()->currentStageId();
	}
	setCurrentStageId(stage_id);
}

void EventPlugin::saveCurrentStageId(int current_stage)
{
	if(current_stage != eventConfig()->currentStageId()) {
		eventConfig()->setValue("event.currentStageId", current_stage);
		eventConfig()->save("event");
	}
}

void EventPlugin::editStage()
{
	//qfLogFuncFrame();// << "id:" << id << "mode:" << mode;
	int stage_id = currentStageId();
	auto *w = new Event::StageWidget();
	auto *fwk = qf::gui::framework::MainWindow::frameWork();
	qfd::Dialog dlg(QDialogButtonBox::Save | QDialogButtonBox::Cancel, fwk);
	dlg.setDefaultButton(QDialogButtonBox::Save);
	dlg.setCentralWidget(w);
	w->load(stage_id);
	if(dlg.exec()) {
		emitDbEvent(Event::EventPlugin::DBEVENT_STAGE_START_CHANGED, stage_id, true);
	}
}

void EventPlugin::emitDbEvent(const QString &domain, const QVariant &data, bool loopback)
{
	qfLogFuncFrame() << "domain:" << domain << "payload:" << data;
	int connection_id = qf::core::sql::Connection::defaultConnection().connectionId();
	if(loopback) {
		// emit queued
		QTimer::singleShot(0, this, [this, domain, connection_id, data]() {
			emit dbEventNotify(domain, connection_id, data);
		});
	}
	if(isSingleUser()) {
		return;
	}
	DbEventPayload dbpl;
	dbpl.setEventName(eventName());
	dbpl.setDomain(domain);
	dbpl.setData(data);
	dbpl.setconnectionId(connection_id);
	QByteArray json_ba = dbpl.toJson();
	QString payload_str = QString::fromUtf8(json_ba);
	if(payload_str.length() > 4000) {
		int len = payload_str.toUtf8().length();
		if(len > 8000) {
			qfInfo() << payload_str;
			qfError() << "Payload of size" << len << "is too long. Max Postgres payload length is 8000 bytes.";
			return;
		}
	}
	payload_str = qf::core::sql::Connection::escapeJsonForSql(payload_str);
	qf::core::sql::Connection conn = qf::core::sql::Connection::forName();
	QString qs = QString("NOTIFY ") + DBEVENT_NOTIFY_NAME + ", '" + payload_str + "'";
	qfDebug() << conn.driver() << "executing SQL:" << qs;
	QSqlQuery q(conn);
	if(!q.exec(qs)) {
		qfError() << "emitDbEventNotify Error:" << qs << q.lastError().text();
	}
}

QString EventPlugin::sqlDriverName()
{
	qf::core::sql::Connection cc = qf::core::sql::Connection::forName();
	return cc.driverName();
}

QString EventPlugin::classNameById(int class_id)
{
	if(m_classNameCache.isEmpty()) {
		qf::core::sql::Query q;
		q.exec("SELECT id, name FROM classes");
		while (q.next()) {
			m_classNameCache[q.value(0).toInt()] = q.value(1).toString();
		}
	}
	return m_classNameCache.value(class_id);
}

QString EventPlugin::shvApiEventId() const
{
	return eventName() + "-" + QString::number(m_eventConfig->importId());
}

QString EventPlugin::createApiKey(int length)
{
	QString key;
	static const QList<char> vowels{'a', 'e', 'i', 'o', 'u', 'y'};
	static const QList<char> consonants = []() {
		QList<char> cc;
		for (auto i = 'a'; i <= 'z'; i++) {
			if (!vowels.contains(i)) {
				cc << i;
			}
		}
		return cc;
	} ();
	for (int i = 0; i < length; i++) {
		if (i % 2 == 0) {
			auto ix = QRandomGenerator::global()->generate() % consonants.size();
			key += consonants[ix];
		}
		else {
			auto ix = QRandomGenerator::global()->generate() % vowels.size();
			key += vowels[ix];
		}
	}
	return key;
}

DbSchema *EventPlugin::dbSchema()
{
	return new DbSchema(this);
}

int EventPlugin::dbVersion()
{
	// equals to minimal app version compatible with this DB
	return 30300;
}

QString EventPlugin::dbVersionString()
{
	int dbv = dbVersion();
	int rev = dbv % 100;
	dbv /= 100;
	int min = dbv % 100;
	int maj = dbv / 100;

	return QString("%1.%2.%3").arg(maj).arg(min).arg(rev);
}

void EventPlugin::onDbEvent(const QString &name, QSqlDriver::NotificationSource source, const QVariant &payload)
{
	qfLogFuncFrame() << "name:" << name << "source:" << source << "payload:" << payload;
	if(name == QLatin1String(DBEVENT_NOTIFY_NAME)) {
		if(source == QSqlDriver::OtherSource) {
			DbEventPayload dbpl = DbEventPayload::fromJson(payload.toString().toUtf8());
			QString domain = dbpl.domain();
			if(domain.isEmpty()) {
				qfWarning() << "DbNotify with invalid domain, payload:" << payload.toString();
				return;
			}
			QString event_name = dbpl.eventName();
			if(event_name.isEmpty()) {
				qfWarning() << "DbNotify with invalid event name, payload:" << event_name << payload.toString();
				return;
			}
			if(event_name == eventName()) {
				QVariant data = dbpl.data();
				qfDebug() << "emitting domain:" << domain << "data:" << data;
				emit dbEventNotify(domain, dbpl.connectionId(), data);
			}
		}
		else {
			//qfDebug() << "self db notify";
		}
	}
}

void EventPlugin::onRegistrationsDockVisibleChanged(bool on)
{
	if(on && !m_registrationsDockWidget->widget()) {
		auto *rw = new RegistrationsWidget();
		m_registrationsDockWidget->setWidget(rw);
		rw->checkModel();
	}
}
void EventPlugin::repairStageStarts(const qf::core::sql::Connection &from_conn, const qf::core::sql::Connection &to_conn)
{
	qfs::Query to_q(to_conn);
	qfs::Query from_q(from_conn);
	from_q.exec("SELECT * FROM stages ORDER BY id");
	while(from_q.next()) {
		int ix = from_q.fieldIndex(QStringLiteral("startDate"));
		if(ix < 0)
			break;
		QDate d = from_q.value(ix).toDate();
		QTime t = from_q.value("startTime").toTime();
		QDateTime dt(d, t);
		int id = from_q.value("id").toInt();
		to_q.exec("UPDATE stages SET startDateTime=" QF_SARG(dt.toString(Qt::ISODate)) " WHERE id=" QF_IARG(id));
	}
}

void EventPlugin::onEventOpened()
{
	if(!isEventOpen())
		return;
	qfLogFuncFrame() << "stage count:" << stageCount();
	{
		QSignalBlocker b(m_cbxStage);
		m_cbxStage->clear();
		int stage_cnt = stageCount();
		for (int i = 0; i < stage_cnt; ++i) {
			m_cbxStage->addItem("E" + QString::number(i + 1), i + 1);
		}
		m_cbxStage->setCurrentIndex(-1);
	}
	loadCurrentStageId();
}

EventPlugin::ConnectionType EventPlugin::connectionType() const
{
	ConnectionSettings connection_settings;
	return connection_settings.connectionType();
}

bool EventPlugin::isSingleUser() const
{
	return connectionType() == ConnectionType::SingleFile;
}

QStringList EventPlugin::existingSqlEventNames() const
{
	qfs::Connection conn(QSqlDatabase::database());
	qfs::Query q(conn);
	qfs::QueryBuilder qb;
	QStringList event_names;
	{
		qb.select("nspname")
				.from("pg_catalog.pg_namespace  AS n")
				.where("nspname NOT LIKE 'pg\\_%'")
				.where("nspname NOT IN ('public', 'information_schema')")
				.orderBy("nspname");
		q.exec(qb.toString());
		while(q.next()) {
			event_names << q.value("nspname").toString();
		}
	}
	return event_names;
}

QStringList EventPlugin::existingFileEventNames(const QString &_dir) const
{
	/*
	QStringList ret;
	QDirIterator it(dir);
	while (it.hasNext()) {
		if(it.fileName().endsWith(QLatin1String(".qbe"), Qt::CaseInsensitive))
			ret << it.fileName().mid(0, it.fileName().length() - 4);
	}
	return ret;
	*/
	QString dir = _dir;
	if(dir.isEmpty()) {
		ConnectionSettings connection_settings;
		dir = connection_settings.singleWorkingDir();
	}
	QDir working_dir(dir);
	QStringList event_names = working_dir.entryList(QStringList() << ('*' + QBE_EXT), QDir::Files | QDir::Readable, QDir::Name);
	for (int i = 0; i < event_names.count(); ++i) {
		event_names[i] = fileNameToEventName(event_names[i]);
	}
	return event_names;
}

void EventPlugin::connectToSqlServer()
{
	qfLogFuncFrame();

	qff::MainWindow *fwk = qff::MainWindow::frameWork();

	bool connect_ok = false;
	ConnectionType connection_type = ConnectionType::SingleFile;

	qfd::Dialog dlg(fwk);
	dlg.setButtons(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
	dlg.setDefaultButton(QDialogButtonBox::Ok);
	dlg.setSavePersistentPosition(false);
	auto *conn_w = new ConnectDbDialogWidget();
	dlg.setCentralWidget(conn_w);
	while(!connect_ok) {
		conn_w->loadSettings();
		if(!dlg.exec()) {
			if(!m_sqlServerConnected) {
				qfd::MessageBox::showWarning(fwk, tr("You are not connected to database.\n"
								     "Program features will be limited.\n\n"
								     "To connect to a database or to choose a working directory where event files can be stored, navigate to:\n "
								     "\"File -> Connect to database\" "));
				break;
			}
			return;

		}
		conn_w->saveSettings();
		connection_type = conn_w->connectionType();
		qfDebug() << "connection_type:" << (int)connection_type;
		if(connection_type == ConnectionType::SqlServer) {
			QString driver_name = "QPSQL";
			QSqlDatabase::removeDatabase(QSqlDatabase::defaultConnection);
			QSqlDatabase db = QSqlDatabase::addDatabase(driver_name);
			connect_ok = db.isValid();
			qfInfo() << "Adding database driver:" << driver_name << "OK:" << connect_ok;
			if(connect_ok) {
				//Log.info(db, db.connectionName, db.driverName);
				db.setHostName(conn_w->serverHost());
				db.setPort(conn_w->serverPort());
				db.setUserName(conn_w->serverUser());
				//qfInfo() << conn_w->serverPassword();
				db.setPassword(conn_w->serverPassword());
				db.setDatabaseName("quickevent");
				db.setConnectOptions("connect_timeout=30");
				qfInfo().nospace() << "connecting to database: " << db.databaseName()
								   << " as " << db.userName() << "@" << db.hostName() << ":" << db.port();
				connect_ok = db.open();
				if(connect_ok) {
					bool ok = connect(db.driver(), &QSqlDriver::notification, this, &EventPlugin::onDbEvent);
					if(ok)
						ok = db.driver()->subscribeToNotification(DBEVENT_NOTIFY_NAME);
					if(!ok)
						qfError() << "Failed to subscribe db notification:" << DBEVENT_NOTIFY_NAME;
					else {
						qfInfo() << "Successfully subscribe db notification:" << DBEVENT_NOTIFY_NAME;
						qfInfo() << db.driver() << "subscribedToNotifications:" << db.driver()->subscribedToNotifications().join(", ");
					}
				}
			}
			if(!connect_ok) {
				qff::MainWindow *fwk = qff::MainWindow::frameWork();
				qfd::MessageBox::showError(fwk, tr("Connect Database Error: %1").arg(db.lastError().text()));
			}
		}
		else {
			QString single_working_dir = conn_w->singleWorkingDir();
			bool swd_empty = single_working_dir.isEmpty();
			if(swd_empty) {
				qfd::MessageBox::showError(fwk, tr("Path to the working directory cannot be empty.\n\nEnter path to the working directory or connect to SQL server."));
			}
			bool swd_exist = QDir (single_working_dir).exists();
			if(!swd_exist) {
				qfd::MessageBox::showError(fwk, tr("Entered directory does not exist:\n%1\n\nEnter a valid path to the working directory.").arg(single_working_dir));
			}
			if(!swd_empty && swd_exist) {
				QString driver_name = "QSQLITE";
				QSqlDatabase::removeDatabase(QSqlDatabase::defaultConnection);
				QSqlDatabase db = QSqlDatabase::addDatabase(driver_name);
				connect_ok = true;
			}
		}
	}
	setSqlServerConnected(connect_ok);
	m_actEvent->setEnabled(connect_ok);
	m_actExport->setEnabled(connect_ok);
	m_actImport->setEnabled(connect_ok);
	m_actEditStage->setEnabled(connect_ok);
	if(connect_ok) {
		closeEvent();
		openEvent(conn_w->eventName());
	}
}
namespace {
bool run_sql_script(qf::core::sql::Query &q, const QStringList &sql_lines)
{
	qfLogFuncFrame();
	QVariantMap replacements;
	replacements["minDbVersion"] = EventPlugin::dbVersion();
	for(auto cmd : sql_lines) {
		if(cmd.isEmpty())
			continue;
		if(cmd.startsWith(QLatin1String("--")))
			continue;
		qfDebug() << cmd << ';';
		cmd = qf::core::Utils::replaceCaptions(cmd, replacements);
		bool ok = q.exec(cmd);
		if(!ok) {
			qfInfo() << cmd;
			qfError() << q.lastError().text();
			return false;
		}
	}
	return true;
}
}
bool EventPlugin::createEvent(const QString &event_name, const QVariantMap &event_params)
{
	qfLogFuncFrame();

	qff::MainWindow *fwk = qff::MainWindow::frameWork();
	EventPlugin::ConnectionType connection_type = connectionType();
	QStringList existing_event_ids;
	if(connection_type == ConnectionType::SingleFile)
		existing_event_ids = existingFileEventNames();
	else
		existing_event_ids = existingSqlEventNames();
	QString event_id = event_name;
	QVariantMap new_params = event_params;
	do {
		qfd::Dialog dlg(fwk);
		dlg.setButtons(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
		auto *event_w = new EventDialogWidget();
		event_w->setWindowTitle(tr("Create event"));
		event_w->setEventId(event_id);
		event_w->loadParams(new_params);
		dlg.setCentralWidget(event_w);
		if(!dlg.exec())
			return false;

		event_id = event_w->eventId();
		new_params = event_w->saveParams();
		if(event_id.isEmpty()) {
			qf::gui::dialogs::MessageBox::showError(fwk, tr("Event ID cannot be empty."));
			continue;
		}
		if(existing_event_ids.contains(event_id)) {
			qf::gui::dialogs::MessageBox::showError(fwk, tr("Event ID %1 exists already.").arg(event_id));
			continue;
		}
		break;
	} while(true);

	closeEvent();

	Event::EventConfig event_config;
	bool ok = false;
	//ConnectionSettings connection_settings;
	event_config.setValue("event", new_params);
	int stage_count = event_params.value("stageCount").toInt();
	if(stage_count == 0)
		stage_count = event_config.stageCount();
	qfInfo() << "createEvent, stage_count:" << stage_count;
	QF_ASSERT(stage_count > 0, "Stage count have to be greater than 0", return false);

	qfInfo() << "will create DB:" << event_id;
	qfs::Connection conn = qfs::Connection::forName();
	//QF_ASSERT(conn.isOpen(), "Connection is not open", return false);
	if(connection_type == ConnectionType::SingleFile) {
		QString event_fn = eventNameToFileName(event_id);
		conn.close();
		conn.setDatabaseName(event_fn);
		qfInfo() << "opening file:" << conn.databaseName() << "driver:" << conn.driverName();
		if(!conn.open()) {
			qfd::MessageBox::showError(fwk, tr("Open Database Error: %1").arg(conn.errorString()));
			return false;
		}
	}
	if(conn.isOpen()) {
		QVariantMap create_options;
		create_options["schemaName"] = event_id;
		create_options["driverName"] = conn.driverName();

		QStringList create_script = dbSchema()->createDbSqlScript(create_options);

		qfInfo().nospace() << create_script.join(";\n") << ';';
		qfs::Query q(conn);
		do {
			qfLogScope("createEvent");
			qfs::Transaction transaction(conn);
			ok = run_sql_script(q, create_script);
			if(!ok)
				break;
			qfDebug() << "creating stages:" << stage_count;
			QString stage_table_name = "stages";
			if(connection_type == ConnectionType::SqlServer)
				stage_table_name = event_id + '.' + stage_table_name;
			QDateTime start_dt = event_config.eventDateTime();
			// FIXME: handle SQL errors here and below in q.exec()
			q.prepare("INSERT INTO " + stage_table_name + " (id, startDateTime) VALUES (:id, :startDateTime)");
			for(int i=0; i<stage_count; i++) {
				q.bindValue(":id", i+1);
				q.bindValue(":startDateTime", start_dt);
				ok = q.exec();
				if(!ok) {
					break;
				}
				start_dt = start_dt.addDays(1);
			}
			if(!ok)
				break;
			conn.setCurrentSchema(event_id);
			event_config.save();
			transaction.commit();
		} while(false);
		if(!ok) {
			qfd::MessageBox::showError(fwk, tr("Create Database Error: %1").arg(q.lastError().text()));
		}
	}
	else {
		qfd::MessageBox::showError(fwk, tr("Cannot create event, database is not open: %1").arg(conn.lastError().text()));
	}
	if(ok) {
		ok = openEvent(event_id);
	}
	return ok;
}

void EventPlugin::editEvent()
{
	qfLogFuncFrame();
	qff::MainWindow *fwk = qff::MainWindow::frameWork();
	qfd::Dialog dlg(fwk);
	dlg.setButtons(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
	auto *event_w = new EventDialogWidget();
	event_w->setWindowTitle(tr("Edit event"));
	event_w->setEventId(eventName());
	event_w->setEventIdEditable(false);
	event_w->loadParams(eventConfig()->value("event").toMap());
	dlg.setCentralWidget(event_w);
	if(!dlg.exec())
		return;

	eventConfig()->setValue("event", event_w->saveParams());
	eventConfig()->save("event");
}

bool EventPlugin::closeEvent()
{
	qfLogFuncFrame();
	clearStageDataCache();
	m_classNameCache.clear();
	setEventName(QString());
	QF_SAFE_DELETE(m_eventConfig)
	setEventOpen(false);
	return true;
}

bool EventPlugin::openEvent(const QString &_event_name)
{
	qff::MainWindow *fwk = qff::MainWindow::frameWork();
	QString event_name = _event_name;
	QStringList db_event_names = QStringList();
	QString empty_message;
	ConnectionSettings connection_settings;
	ConnectionType connection_type = connection_settings.connectionType();
	//console.debug("openEvent()", "event_name:", event_name, "connection_type:", connection_type);
	bool ok = true;
	switch (connection_type) {
		case ConnectionType::SqlServer:
			db_event_names = existingSqlEventNames();
			empty_message = tr("Connected to an empty database.\nStart by creating or importing an event.");
			break;
		case ConnectionType::SingleFile:
			db_event_names = existingFileEventNames(connection_settings.singleWorkingDir());
			empty_message = tr("Working directory does not contain any event files.\nStart by creating or importing an event.");
			break;
	}
	if(db_event_names.isEmpty()) {
		// openEvent function was called on empty database
		qfd::MessageBox::showInfo(fwk, empty_message);
		ok = false;
	}
	else if (!db_event_names.contains(event_name)) {
		// database does not contain given event_name => ask which event to open
		event_name = QInputDialog::getItem(fwk, tr("Open event"), tr("select event to open:"), db_event_names, 0, false, &ok);
	}
	// if given event_name is in the db, preceeding conditions were skipped => ok => open event_name
	// if dialog was succesfull => ok => open event_name
	// if dialog was canceled => !ok => close event and disable menu options
	if(!eventName().isEmpty() && db_event_names.contains(eventName()) && !ok) // open event dialog was canceled and event is already opened => no change, return
		return true;

	closeEvent();

	if(ok) {
		if(connection_type == ConnectionType::SqlServer) {
			qfs::Connection conn(QSqlDatabase::database());
			if(conn.setCurrentSchema(event_name)) {
				ConnectionSettings settings;
				settings.setEventName(event_name);
			}
		}
		else {
			QString event_fn = eventNameToFileName(event_name);
			if(QFile::exists(event_fn)) {
				{
					QString conn_name;
					{
						qfs::Connection conn(QSqlDatabase::database());
						conn_name = conn.connectionName();
						conn.close();
					}
					qfInfo() << "removing database:" << conn_name;
					QSqlDatabase::removeDatabase(conn_name);
					QSqlDatabase::addDatabase("QSQLITE");
				}
				qfs::Connection conn(QSqlDatabase::database());
				conn.setDatabaseName(event_fn);
				qfInfo() << "Opening database file" << event_fn;
				if(conn.open()) {
					qfs::Query q(conn);
					ok = q.exec("PRAGMA foreign_keys=ON");
				}
				else {
					qfd::MessageBox::showError(fwk, tr("Open Database Error: %1").arg(conn.errorString()));
					ok = false;
				}
			}
			else {
				qfd::MessageBox::showError(fwk, tr("Database file %1 doesn't exist.").arg(event_fn));
				ok = false;
			}

		}
	}
	if(ok) {
		EventConfig *evc = eventConfig(true);
		if(evc->dbVersion() < dbVersion()) {
			qfd::MessageBox::showError(fwk, tr("Event data version (%1) is too low, minimal version is (%2).\nUse: File --> Import --> Event (*.qbe) to convert event to current version.")
									   .arg(qf::core::Utils::intToVersionString(evc->dbVersion()))
									   .arg(qf::core::Utils::intToVersionString(dbVersion())));
			closeEvent();
			ok = false;
		}
		else if(evc->dbVersion() > dbVersion()) {
			qfd::MessageBox::showError(fwk, tr("Event was created in more recent QuickEvent version (%1) and the application might not work as expected. Download latest QuickEvent is strongly recommended.")
									   .arg(qf::core::Utils::intToVersionString(evc->dbVersion())));
		}
	}
	if(ok) {
		connection_settings.setEventName(event_name);
		setEventName(event_name);
		//emit reloadDataRequest();
	}
	m_actEditStage->setEnabled(ok);
	m_actOpenEvent->setEnabled(ok || !db_event_names.isEmpty());
	m_actEditEvent->setEnabled(ok);
	m_actExportEvent_qbe->setEnabled(ok);
	setEventOpen(ok);
	return ok;
}

void EventPlugin::setSqlServerConnected(bool ok)
{
	if(ok != m_sqlServerConnected) {
		m_sqlServerConnected = ok;
		emit sqlServerConnectedChanged(ok);
	}
}

namespace {
QString copy_sql_table(const QString &table_name, const QSqlRecord &dest_rec, qfs::Connection &from_conn, qfs::Connection &to_conn)
{
	qfLogFuncFrame() << table_name;
	qfInfo() << "Copying table:" << table_name;
	if(!to_conn.tableExists(table_name)) {
		qfWarning() << "Destination table" << table_name << "doesn't exist!";
		return QString();
	}
	qfs::Query from_q(from_conn);
	if(!from_q.exec(QString("SELECT * FROM %1").arg(table_name))) {
		qfWarning() << "Source table" << table_name << "doesn't exist!";
		return QString();
	}
	const QSqlRecord src_rec = from_q.record();

	auto dest_db_version = EventPlugin::dbVersion();
	bool is_import_runs_table = table_name == QLatin1String("runs");
	// copy only fields which can be found in both records
	QSqlRecord rec;
	for (int i = 0; i < dest_rec.count(); ++i) {
		QString fld_name = dest_rec.fieldName(i);
		if (is_import_runs_table && fld_name == "disqualified" && dest_db_version >= 30100) {
			// disqualified field is autogenerated since 3.1.0
			continue;
		}
		if(src_rec.indexOf(fld_name) >= 0) {
			qfDebug() << fld_name << "\t added to imported fields since it is present in both databases";
			rec.append(dest_rec.field(i));
		}
	}
	bool is_import_offrace = false;
	if(is_import_runs_table) {
		is_import_runs_table = true;
		if(!src_rec.contains("isRunning") && dest_rec.contains("isRunning") && src_rec.contains("offRace")) {
			is_import_offrace = true;
			rec.append(dest_rec.field("isRunning"));
		}
	}
	auto *sqldrv = to_conn.driver();
	QString qs = sqldrv->sqlStatement(QSqlDriver::InsertStatement, table_name, rec, true);
	qfDebug() << qs;
	qfs::Query to_q(to_conn);
	if(!to_q.prepare(qs)) {
		QString ret = QString("Cannot prepare insert table SQL statement, table: %1.\n%2").arg(table_name).arg(to_q.lastErrorText());
		qfInfo() << qs;
		return ret;
	}
	bool has_id_int = false;
	while(from_q.next()) {
		if(table_name == QLatin1String("config")) {
			if(from_q.value(0).toString() == QLatin1String("db.version"))
				continue;
		}
		for (int i = 0; i < rec.count(); ++i) {
			QSqlField fld = rec.field(i);
			QString fld_name = fld.name();
			//qfDebug() << "copy:" << fld_name << from_q.value(fld_name);
			QVariant v;
			if(is_import_runs_table && fld_name.compare(QLatin1String("isRunning"), Qt::CaseInsensitive) == 0) {
				if(is_import_offrace) {
					bool offrace = from_q.value(QStringLiteral("offRace")).toBool();
					v = offrace? QVariant(): QVariant(true);
				}
				else {
					// since db ver 1.8.0
					// isRunning cannot be NULL, convert NULL to false during import
					v = from_q.value(fld_name).toBool();
				}
			}
			else {
				v = from_q.value(fld_name);
				v.convert(rec.field(i).metaType());
			}
			if(!has_id_int
					&& (fld.metaType().id() == QMetaType::Type::Int
						|| fld.metaType().id() == QMetaType::Type::UInt
						|| fld.metaType().id() == QMetaType::Type::LongLong
						|| fld.metaType().id() == QMetaType::Type::ULongLong)
					&& fld_name == QLatin1String("id")) {
				// probably ID INT AUTO_INCREMENT
				//max_id = qMax(max_id, v.toInt());
				has_id_int = true;
			}
			to_q.addBindValue(v);
		}
		if(!to_q.exec())
			return QString("SQL Error: %1").arg(to_q.lastError().text());
	}
	if(has_id_int && to_conn.driverName().endsWith(QLatin1String("PSQL"), Qt::CaseInsensitive)) {
		// set sequence current value when importing to PSQL
		qfInfo() << "updating seq number table:" << table_name;
		if(!to_q.exec("SELECT pg_catalog.setval(pg_get_serial_sequence(" QF_SARG(table_name) ", 'id'), MAX(id)) FROM " QF_CARG(table_name), !qf::core::Exception::Throw)) {
			return QString("Cannot update sequence counter, table: %1.").arg(table_name);
		}
	}
	return QString();
}
}
void EventPlugin::exportEvent_qbe()
{
	qfLogFuncFrame();
	qff::MainWindow *fwk = qff::MainWindow::frameWork();
	QString ext = ".qbe";
	QString ex_fn = qf::gui::dialogs::FileDialog::getSaveFileName (fwk, tr("Export as Quick Event"), singleFileStorageDir(), tr("Quick Event files *%1 (*%1)").arg(ext));
	if(ex_fn.isEmpty())
		return;
	if(!ex_fn.endsWith(ext, Qt::CaseInsensitive))
		ex_fn += ext;
	QString err_str;
	QString export_connection_name = QStringLiteral("qe_export_connection");
	do {
		if(QFile::exists(ex_fn)) {
			if(!QFile::remove(ex_fn)) {
				err_str = tr("Cannot delete existing file %1").arg(ex_fn);
				break;
			}
		}
		qfs::Connection ex_conn(QSqlDatabase::addDatabase("QSQLITE", export_connection_name));
		ex_conn.setDatabaseName(ex_fn);
		qfInfo() << "Opening export database file" << ex_fn;
		if(!ex_conn.open()) {
			qfd::MessageBox::showError(fwk, tr("Open Database Error: %1").arg(ex_conn.errorString()));
			return;
		}
		//qfLogScope("exportEvent_qbe");
		qfs::Transaction transaction(ex_conn);

		DbSchema *db_schema = dbSchema();
		auto tables = db_schema->tables();
		int step_cnt = tables.count() + 1;
		int step_no = 0;
		fwk->showProgress(tr("Creating database"), ++step_no, step_cnt);
		{
			DbSchema::CreateDbSqlScriptOptions create_options;
			create_options.setDriverName(ex_conn.driverName());
			QStringList create_script = db_schema->createDbSqlScript(create_options);
			qfs::Query ex_q(ex_conn);
			if(!run_sql_script(ex_q, create_script)) {
				err_str = tr("Create Database Error: %1").arg(ex_q.lastError().text());
				break;
			}
		}
		qfs::Connection conn = qfs::Connection::forName();
		for(QObject *table : tables) {
			QString table_name = table->property("name").toString();
			qfDebug() << "Copying table" << table_name;
			fwk->showProgress(tr("Copying table %1").arg(table_name), ++step_no, step_cnt);
			QSqlRecord rec = db_schema->sqlRecord(table);
			err_str = copy_sql_table(table_name, rec, conn, ex_conn);
			if(!err_str.isEmpty())
				break;
		}
		if(!err_str.isEmpty())
			break;
		transaction.commit();
	} while(false);
	{
		QSqlDatabase c = QSqlDatabase::database(export_connection_name, false);
		if(c.isOpen())
			c.close();
	}
	QSqlDatabase::removeDatabase(export_connection_name);
	fwk->hideProgress();
	if(!err_str.isEmpty()) {
		qfd::MessageBox::showError(fwk, err_str);
	}
}

void EventPlugin::importEvent_qbe()
{
	qfLogFuncFrame();
	qff::MainWindow *fwk = qff::MainWindow::frameWork();
	QString ext = ".qbe";
	QString fn = qf::gui::dialogs::FileDialog::getOpenFileName (fwk, tr("Import as Quick Event"), QString(), tr("Quick Event files *%1 (*%1)").arg(ext));
	if(fn.isEmpty())
		return;
	QString event_name = qf::core::utils::FileUtils::baseName(fn) + "_2";
	event_name = QInputDialog::getText(fwk, tr("Query"), tr("Event will be imported as ID:"), QLineEdit::Normal, event_name).trimmed();
	if(event_name.isEmpty())
		return;
	const std::regex psqlschema_regex("[a-z][a-z0-9_]*");
	if(connectionType() == ConnectionType::SqlServer && !std::regex_match(event_name.toStdString(), psqlschema_regex)) {
		qfd::MessageBox::showError(fwk, tr("PostgreSQL schema must start with small letter and it may contain small letters, digits and underscores only."));
		return;
	}
	QStringList existing_events = (connectionType() == ConnectionType::SingleFile)? existingFileEventNames(): existingSqlEventNames();
	if(existing_events.contains(event_name)) {
		qfd::MessageBox::showError(fwk, tr("Event ID '%1' exists already!").arg(event_name));
		return;
	}

	QString err_str;
	QString import_connection_name = QStringLiteral("qe_import_connection");
	QString export_connection_name = QStringLiteral("qe_export_connection");
	do {
		qfs::Connection current_conn = qfs::Connection::forName();

		qfs::Connection imp_conn(QSqlDatabase::addDatabase("QSQLITE", import_connection_name));
		imp_conn.setDatabaseName(fn);
		qfInfo() << "Opening import database file" << fn;
		if(!imp_conn.open()) {
			qfd::MessageBox::showError(fwk, tr("Open Database Error: %1").arg(imp_conn.errorString()));
			return;
		}

		qfs::Connection exp_conn(QSqlDatabase::addDatabase(current_conn.driverName(), export_connection_name));
		if(connectionType() == ConnectionType::SingleFile) {
			exp_conn.setDatabaseName(eventNameToFileName(event_name));
		}
		else {
			exp_conn.setHostName(current_conn.hostName());
			exp_conn.setPort(current_conn.port());
			exp_conn.setUserName(current_conn.userName());
			exp_conn.setPassword(current_conn.password());
			exp_conn.setDatabaseName(current_conn.databaseName());
		}
		qfInfo() << "Opening export database:" << exp_conn.databaseName();
		if(!exp_conn.open()) {
			qfd::MessageBox::showError(fwk, tr("Open Database Error: %1").arg(exp_conn.errorString()));
			return;
		}

		qfs::Transaction transaction(exp_conn);

		DbSchema *db_schema = dbSchema();
		auto tables = db_schema->tables();
		int step_cnt = tables.count() + 1;
		int step_no = 0;
		fwk->showProgress(tr("Creating database"), ++step_no, step_cnt);
		{
			DbSchema::CreateDbSqlScriptOptions create_options;
			create_options.setDriverName(exp_conn.driverName());
			create_options.setSchemaName(event_name);
			QStringList create_script = db_schema->createDbSqlScript(create_options);
			qfs::Query ex_q(exp_conn);
			if(!run_sql_script(ex_q, create_script)) {
				err_str = tr("Create Database Error: %1").arg(ex_q.lastError().text());
				break;
			}
		}
		exp_conn.setCurrentSchema(event_name);
		for(QObject *table : tables) {
			QString table_name = table->property("name").toString();
			qfDebug() << "Copying table" << table_name;
			fwk->showProgress(tr("Copying table %1").arg(table_name), ++step_no, step_cnt);
			QSqlRecord rec = db_schema->sqlRecord(table, true);
			err_str = copy_sql_table(table_name, rec, imp_conn, exp_conn);
			if(!err_str.isEmpty())
				break;
			if(table_name == QLatin1String("stages")) {
				repairStageStarts(imp_conn, exp_conn);
			}
		}
		if(!err_str.isEmpty())
			break;
		transaction.commit();
	} while(false);
	QSqlDatabase::removeDatabase(import_connection_name);
	QSqlDatabase::removeDatabase(export_connection_name);
	fwk->hideProgress();
	if(!err_str.isEmpty()) {
		qfd::MessageBox::showError(fwk, err_str);
		return;
	}
	if(qfd::MessageBox::askYesNo(fwk, tr("Open imported event '%1'?").arg(event_name), false)) {
		openEvent(event_name);
	}
}

void EventPlugin::onServiceDockVisibleChanged(bool on)
{
	if(on && !m_servicesDockWidget->widget()) {
		auto *rw = new services::ServicesWidget();
		m_servicesDockWidget->setWidget(rw);
		rw->reload();
	}
}

void EventPlugin::onDbEventNotify(const QString &domain, int connection_id, const QVariant &data)
{
	Q_UNUSED(connection_id)
	qfLogFuncFrame() << "domain:" << domain << "payload:" << data;
	if(domain == QLatin1String(Event::EventPlugin::DBEVENT_STAGE_START_CHANGED)) {
		//int stage_id = data.toInt();
		clearStageDataCache();
	}
	else if(domain == QLatin1String(Event::EventPlugin::DBEVENT_REGISTRATIONS_IMPORTED)) {
		reloadRegistrationsModel();
	}
}

void EventPlugin::reloadRegistrationsModel()
{
	qfLogFuncFrame() << "isEventOpen():" << getPlugin<EventPlugin>()->isEventOpen();
	if(getPlugin<EventPlugin>()->isEventOpen())
		registrationsModel()->reload();
	else
		registrationsModel()->clearRows();
	// clear registration table to be regenerated when registrationsTable() will be called
	m_registrationsTable = qf::core::utils::Table();
}

qf::gui::model::SqlTableModel* EventPlugin::registrationsModel()
{
	if(!m_registrationsModel) {
		m_registrationsModel = new qf::gui::model::SqlTableModel(this);
		m_registrationsModel->addColumn("competitorName", tr("Name"));
		m_registrationsModel->addColumn("registration", tr("Reg"));
		m_registrationsModel->addColumn("licence", tr("Lic"));
		m_registrationsModel->addColumn("siId", tr("SI"));
		//m_registrationsModel->addColumn("fistName");
		//m_registrationsModel->addColumn("lastName");
		qfs::QueryBuilder qb;
		qb.select2("registrations", "firstName, lastName, licence, registration, siId")
				.select("COALESCE(lastName, '') || ' ' || COALESCE(firstName, '') AS competitorName")
				.from("registrations")
				.orderBy("lastName, firstName");
		m_registrationsModel->setQueryBuilder(qb, false);
	}
	return m_registrationsModel;
}

const qf::core::utils::Table &EventPlugin::registrationsTable()
{
	qf::gui::model::SqlTableModel *m = registrationsModel();
	if(m_registrationsTable.isNull() && !m->table().isNull()) {
		m_registrationsTable = m->table();
		auto c_nsk = QStringLiteral("competitorNameAscii7");
		m_registrationsTable.appendColumn(c_nsk, QMetaType::QString);
		int ix_nsk = m_registrationsTable.fields().fieldIndex(c_nsk);
		int ix_cname = m_registrationsTable.fields().fieldIndex(QStringLiteral("competitorName"));
		for (int i = 0; i < m_registrationsTable.rowCount(); ++i) {
			qf::core::utils::TableRow &row_ref = m_registrationsTable.rowRef(i);
			QString nsk = row_ref.value(ix_cname).toString();
			nsk = QString::fromLatin1(qf::core::Collator::toAscii7(QLocale::Czech, nsk, true));
			row_ref.setValue(ix_nsk, nsk);
		}
	}
	return m_registrationsTable;
}

}

