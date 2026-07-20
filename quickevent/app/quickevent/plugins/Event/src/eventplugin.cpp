#include "eventplugin.h"
#include "connectdbdialogwidget.h"
#include "connectionsettings.h"
#include "eventdialogwidget.h"
#include "openeventdialog.h"
#include "dbschema.h"
#include "plugins/Event/src/appdbconfig.h"
#include "plugins/Event/src/eventconfig.h"
#include "plugins/Event/src/stageconfig.h"
#include "registrationswidget.h"
#include "lentcardssettingspage.h"
#include "../../Core/src/widgets/appstatusbar.h"

#include "services/serviceswidget.h"
#include "services/emmaclient.h"
#include "services/qx/qxeventservice.h"
#include "services/punchingtest/punchingtestservice.h"

#include <plugins/Core/src/widgets/settingsdialog.h>
#include <plugins/Event/src/services/oresultsclient.h>
#include <plugins/Core/src/coreplugin.h>
#include <plugins/Event/src/services/ofeed/ofeedclient.h>

#include <qhashfunctions.h>
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
#include <qf/gui/model/sqltablemodel.h>

#include <qf/core/log.h>
#include <qf/core/assert.h>
#include <qf/core/sql/qxsql.h>
#include <qf/core/sql/query.h>
#include <qf/core/sql/querybuilder.h>
#include <qf/core/sql/connection.h>
#include <qf/core/sql/transaction.h>
#include <qf/core/utils/fileutils.h>

#include <QInputDialog>
#include <QDate>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QSqlRecord>
#include <QSqlField>
#include <QSqlError>
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

namespace {

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

QList<OpenEventDialog::EventInfo> loadEventInfoList(EventPlugin::ConnectionType type, const QStringList &event_names)
{
	static const QLatin1String SQL_KEYS =
		QLatin1String("'event.name','event.date','event.sportId','event.disciplineId','db.version'");

	auto parseRow = [](OpenEventDialog::EventInfo &info, const QString &key, const QString &val) {
		if      (key == QLatin1String("event.name"))         info.name         = val;
		else if (key == QLatin1String("event.date"))         info.date         = QDate::fromString(val, Qt::ISODate);
		else if (key == QLatin1String("event.sportId"))      info.sportId      = val.toInt();
		else if (key == QLatin1String("event.disciplineId")) info.disciplineId = val.toInt();
		else if (key == QLatin1String("db.version"))         info.dbVersion    = val.toInt();
	};

	QList<OpenEventDialog::EventInfo> result;
	if (type == EventPlugin::ConnectionType::SingleFile) {
		const QString temp_conn_name = QStringLiteral("qe_eventinfo_conn");
		for (const QString &name : event_names) {
			OpenEventDialog::EventInfo info;
			info.id = name;
			{
				QSqlDatabase db = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), temp_conn_name);
				db.setDatabaseName(eventNameToFileName(name));
				if (db.open()) {
					QSqlQuery q(db);
					q.exec(QStringLiteral("SELECT ckey, cvalue FROM config WHERE ckey IN (%1)").arg(SQL_KEYS));
					while (q.next())
						parseRow(info, q.value(0).toString(), q.value(1).toString());
					db.close();
				}
			}
			QSqlDatabase::removeDatabase(temp_conn_name);
			result << info;
		}
	}
	else {
		// PostgreSQL: use schema-qualified table name to avoid switching search_path
		qfs::Connection conn(QSqlDatabase::database());
		for (const QString &name : event_names) {
			OpenEventDialog::EventInfo info;
			info.id = name;
			qfs::Query q(conn);
			if (q.exec(QStringLiteral("SELECT ckey, cvalue FROM %1.config WHERE ckey IN (%2)").arg(name, SQL_KEYS)))
				while (q.next())
					parseRow(info, q.value(0).toString(), q.value(1).toString());
			result << info;
		}
	}
	return result;
}
}

EventPlugin::EventPlugin(QObject *parent)
	: Super("Event", parent)
{
	connect(this, &EventPlugin::installed, this, &EventPlugin::onInstalled);//, Qt::QueuedConnection);
	connect(this, &EventPlugin::currentStageIdChanged, this, &EventPlugin::saveCurrentStageId);
	connect(this, &EventPlugin::eventDbNameChanged, [this](const QString &event_name) {
		setEventOpen(!event_name.isEmpty());
	});
	connect(this, &Event::EventPlugin::dbEventNotify, this, &Event::EventPlugin::onDbEventNotify, Qt::QueuedConnection);
	connect(qf::gui::framework::Application::instance()->qxSql(), &qf::core::sql::QxSql::recChng, this, &EventPlugin::onRecChng);
}

EventPlugin::~EventPlugin() = default;

Event::AppDbConfig &EventPlugin::appDbConfig()
{
	return m_appDbConfig;
}

const Event::AppDbConfig &EventPlugin::appDbConfig() const
{
	return m_appDbConfig;
}

const EventConfig &EventPlugin::eventConfig() const
{
	return appDbConfig().eventConfig();
}

int EventPlugin::stageCount() const
{
	if(eventDbName().isEmpty()) {
		return 0;
	}
	return appDbConfig().eventConfig().stageCount;
}

const Event::StageConfig &EventPlugin::stageConfig(int stage_id) const
{
	return appDbConfig().stageConfig(stage_id);
}

void EventPlugin::setCurrentStageId(int stage_id)
{
	if(currentStageId() == stage_id) {
		return;
	}
	auto c = eventConfig();
	c.currentStageId = stage_id;
	appDbConfig().setEventConfig(c);
	emit currentStageIdChanged(stage_id);
}

int EventPlugin::currentStageId() const
{
	return eventConfig().currentStageId;
}

int EventPlugin::stageIdForRun(int run_id) const
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

int EventPlugin::stageStartMsec(int stage_id) const
{
	QTime start_time = stageStartTime(stage_id);
	int ret = start_time.msecsSinceStartOfDay();
	return ret;
}

QDate EventPlugin::stageStartDate(int stage_id) const
{
	return stageStartDateTime(stage_id).date();
}

QTime EventPlugin::stageStartTime(int stage_id) const
{
	return stageStartDateTime(stage_id).time();
}

QDateTime EventPlugin::stageStartDateTime(int stage_id) const
{
	return appDbConfig().stageConfig(stage_id).startDateTime;
}

int EventPlugin::msecToStageStartAM(int si_am_time_sec, int msec, int stage_id) const
{
	if(si_am_time_sec == 0xEEEE)
		return quickevent::core::og::TimeMs::UNREAL_TIME_MSEC;
	if(stage_id == 0)
		stage_id = currentStageId();
	int stage_start_msec = stageStartMsec(stage_id);
	int time_msec = quickevent::core::og::TimeMs::msecIntervalAM(stage_start_msec, (si_am_time_sec * 1000) + msec);
	return time_msec;
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
	connect(m_actCreateEvent, &QAction::triggered, this, [this]() { createEvent({}, {}); });

	m_actEditEvent = new qfw::Action(tr("E&dit event"));
	m_actEditEvent->setEnabled(false);
	connect(m_actEditEvent, &QAction::triggered, this, &EventPlugin::editEvent);
	connect(this, &EventPlugin::eventDbNameChanged, [this](const QString &event_name) {
		this->m_actEditEvent->setEnabled(!event_name.isEmpty());
	});

	m_actSetCurrentStage = new qfw::Action(tr("Set current &stage"));
	m_actSetCurrentStage->setEnabled(false);
	connect(m_actSetCurrentStage, &QAction::triggered, this, &EventPlugin::setCurrentStage);
	connect(this, &EventPlugin::eventOpenChanged, m_actSetCurrentStage, &QAction::setEnabled);

	m_actExportEvent_qbe = new qfw::Action(tr("Event (*.qbe)"));
	m_actExportEvent_qbe->setEnabled(false);
	connect(m_actExportEvent_qbe, &QAction::triggered, this, &EventPlugin::exportEvent_qbe);

	m_actImportEvent_qbe = new qfw::Action(tr("Event (*.qbe)"));
	connect(m_actImportEvent_qbe, &QAction::triggered, this, &EventPlugin::importEvent_qbe);

	if(auto *sb = qobject_cast<Core::AppStatusBar*>(fwk->statusBar())) {
		connect(this, &EventPlugin::eventDbNameChanged, sb, &Core::AppStatusBar::setEventDbName);
		connect(this, &EventPlugin::currentStageIdChanged, sb, &Core::AppStatusBar::setStageNo);
		connect(sb, &Core::AppStatusBar::stageClicked, this, &EventPlugin::setCurrentStage);
	}
	connect(this, &EventPlugin::eventDbNameChanged, this, &EventPlugin::updateWindowTitle);
	connect(this, &EventPlugin::currentStageIdChanged, this, &EventPlugin::updateWindowTitle);
	connect(fwk, &qff::MainWindow::applicationLaunched, this, &EventPlugin::connectToSqlServer);

	qfw::Action *a_import = fwk->menuBar()->actionForPath("file/import", false);
	Q_ASSERT(a_import);
	a_import->addActionBefore(m_actConnectDb);
	a_import->addSeparatorBefore();

	qfw::Action *a_file = fwk->menuBar()->actionForPath("file", false);
	Q_ASSERT(a_file);
	m_actEvent = a_file->addMenuAfter("event", tr("&Event"));
	m_actEvent->setEnabled(false);

	m_actEvent->addActionInto(m_actCreateEvent);
	m_actEvent->addActionInto(m_actOpenEvent);
	m_actEvent->addActionInto(m_actEditEvent);
	m_actEvent->addActionInto(m_actSetCurrentStage);
	m_actSetCurrentStage->addSeparatorBefore();

	m_actImport = fwk->menuBar()->actionForPath("file/import");
	m_actImport->addActionInto(m_actImportEvent_qbe);
	m_actImport->setEnabled(false);

	m_actExport = fwk->menuBar()->actionForPath("file/export");
	m_actExport->addActionInto(m_actExportEvent_qbe);
	m_actExport->setEnabled(false);

	qfw::ToolBar *tb = fwk->toolBar("Event", true);
	tb->setObjectName("EventToolbar");
	tb->setWindowTitle(tr("Event"));
	if(auto *part_switch = fwk->findChild<QToolBar *>(QStringLiteral("partSwitch")))
		fwk->insertToolBar(part_switch, tb);
	{
		auto *bt_stage = new QToolButton();
		bt_stage->setAutoRaise(true);
		bt_stage->setEnabled(false);
		bt_stage->setToolTip(tr("Set current stage"));
		tb->addWidget(bt_stage);
		connect(bt_stage, &QToolButton::clicked, this, &EventPlugin::setCurrentStage);
		connect(this, &EventPlugin::eventOpenChanged, bt_stage, &QToolButton::setEnabled);
		connect(this, &EventPlugin::currentStageIdChanged, bt_stage, [bt_stage](int stage_id) {
			bt_stage->setText(tr("Current stage E%1").arg(stage_id));
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

	auto *punching_test = new services::PunchingTestService(this);
	services::Service::addService(punching_test);

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
	QString title = QStringLiteral("%1 E%2").arg(eventDbName()).arg(currentStageId());
	qff::MainWindow *fwk = qff::MainWindow::frameWork();
	fwk->setWindowTitle(title);
}

void EventPlugin::saveCurrentStageId(int current_stage)
{
	if(current_stage != appDbConfig().eventConfig().currentStageId) {
		auto event_cfg = appDbConfig().eventConfig();
		event_cfg.currentStageId = current_stage;
		appDbConfig().setEventConfig(event_cfg);
	}
}

void EventPlugin::setCurrentStage()
{
	QStringList stages;
	for(int stage_id = 1; stage_id <= stageCount(); ++stage_id)
		stages.append(QStringLiteral("E%1").arg(stage_id));
	if(stages.isEmpty())
		return;

	QInputDialog dialog(qff::MainWindow::frameWork());
	dialog.setWindowTitle(tr("Set current stage"));
	dialog.setLabelText(tr("Stage:"));
	dialog.setComboBoxItems(stages);
	dialog.setComboBoxEditable(false);
	dialog.setTextValue(stages.value(currentStageId() - 1, stages.first()));
	if(dialog.exec() == QDialog::Accepted)
		setCurrentStageId(stages.indexOf(dialog.textValue()) + 1);
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
	dbpl.setEventName(eventDbName());
	dbpl.setDomain(domain);
	dbpl.setData(data);
	dbpl.setconnectionId(connection_id);
	QByteArray json_ba = dbpl.toJson();
	QString payload_str = QString::fromUtf8(json_ba);
	if(payload_str.length() > 4000) {
		int len = payload_str.toUtf8().length();
		if(len > 8000) {
			qfError() << "Payload of size" << len << "is too long. Max Postgres payload length is 8000 bytes.";
			return;
		}
	}
	qfMessage() << "to postgres:" << payload_str;
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
	return eventDbName() + "-" + QString::number(eventConfig().importId);
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

QString EventPlugin::fileNameWithStageAndEventName(const QString &fn, std::optional<int> stage_id) const
{
	auto ret = eventDbName();
	if(stageCount() > 1) {
		ret += QStringLiteral(".e%1").arg(stage_id.value_or(currentStageId()));
	}
	ret += '.' + fn;
	return ret;
}

QString EventPlugin::startListIofXml3FileName(std::optional<int> stage_id) const
{
	return fileNameWithStageAndEventName(START_LIST_IOFXML3_FILE, stage_id);
}

QString EventPlugin::resultsIofXml3FileName(std::optional<int> stage_id) const
{
	return fileNameWithStageAndEventName(RESULTS_IOFXML3_FILE, stage_id);
}

DbSchema *EventPlugin::dbSchema()
{
	if (!m_dbSchema) {
		m_dbSchema = new DbSchema(this);
	}
	return m_dbSchema;
}

int EventPlugin::dbVersion()
{
	// equals to app version ignoring patch number
	auto app_ver = QCoreApplication::applicationVersion();
	auto db_ver = (qf::core::Utils::versionStringToInt(app_ver) / 100) * 100;
	return db_ver;
}

QString EventPlugin::dbVersionString()
{
	int dbv = dbVersion();
	dbv /= 100;
	int min = dbv % 100;
	int maj = dbv / 100;

	return QString("%1.%2.0").arg(maj).arg(min);
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
			if(event_name == eventDbName()) {
				QVariant data = dbpl.data();
				if (dbpl.domain() == DBEVENT_QX_RECCHNG) {
					qfMessage() << "from postgres:" << data;
					auto recchng = qf::core::sql::QxRecChng::fromVariantMap(data.toMap());
					if (recchng.issuer == qf::gui::framework::Application::uuidString()) {
						qfWarning() << "RecChng loopback detected, issuer:" << recchng.issuer;
						return;
					}
					qf::gui::framework::Application::instance()->qxSql()->emitRecChng(recchng, this);
					return;
				}
				qfMessage() << "emitting domain:" << domain << "data:" << data;
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
	to_q.prepare("INSERT INTO config (ckey, cvalue, ctype) VALUES (:key, :value, :type)");

	qfs::Query from_q(from_conn);
	from_q.exec("SELECT * FROM stages ORDER BY id", !qf::core::Exception::Throw);
	while(from_q.next()) {
		int ix = from_q.fieldIndex(QStringLiteral("startDateTime"));
		if(ix < 0)
			break;
		auto stage_id = from_q.value("id").toInt();
		StageConfig stage_config;
		stage_config.startDateTime = from_q.value(ix).toDateTime();
		stage_config.drawingConfig = DrawingConfig::fromString(from_q.value("drawingConfig").toString());
		stage_config.useAllMaps = from_q.value("useAllMaps").toBool();
		auto make_key = [stage_id](const QString& key) {return QStringLiteral("stage.%1.%2").arg(stage_id).arg(key);};
		to_q.bindValue(":key", make_key("startDateTime"));
		to_q.bindValue(":value", stage_config.startDateTime);
		to_q.bindValue(":type", "QDateTime");
		to_q.exec();
		to_q.bindValue(":key", make_key("drawingConfig"));
		to_q.bindValue(":value", qf::core::Utils::qvariantToJson(stage_config.drawingConfig.toVariantMap()));
		to_q.bindValue(":type", "QString");
		to_q.exec();
		to_q.bindValue(":key", make_key("useAllMaps"));
		to_q.bindValue(":value", stage_config.useAllMaps);
		to_q.bindValue(":type", "bool");
		to_q.exec();
	}
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

	if(connect_ok) {
		closeEvent();
		openEvent(conn_w->eventName());
	}
}
namespace {
bool run_sql_script(qf::core::sql::Query &q, const QStringList &sql_lines)
{
	qfLogFuncFrame();
	for(const auto &cmd : sql_lines) {
		if(cmd.isEmpty())
			continue;
		if(cmd.startsWith(QLatin1String("--")))
			continue;
		qfDebug() << cmd << ';';
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
bool EventPlugin::createEvent(const QString &event_name, const EventConfig &event_params)
{
	qfLogFuncFrame();

	qff::MainWindow *fwk = qff::MainWindow::frameWork();
	EventPlugin::ConnectionType connection_type = connectionType();
	QStringList existing_event_ids;
	if(connection_type == ConnectionType::SingleFile) {
		existing_event_ids = existingFileEventNames();
	} else {
		existing_event_ids = existingSqlEventNames();
	}
	QString event_id = event_name;
	EventDialogWidget::Params new_params {.eventConfig = event_params, .stageStarts = {}};
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

	bool ok = false;
	//ConnectionSettings connection_settings;
	int stage_count = new_params.eventConfig.stageCount;
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

		QStringList create_script = dbSchema()->loadCreateDbSqlScript(create_options);

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
			if(connection_type == ConnectionType::SqlServer) {
				stage_table_name = event_id + '.' + stage_table_name;
			}
			// Stage details are stored in config, but stage rows are still required by foreign keys.
			q.prepare("INSERT INTO " + stage_table_name + " (id) VALUES (:id)");
			for(int i=0; i<stage_count; i++) {
				q.bindValue(":id", i+1);
				ok = q.exec();
				if(!ok) {
					break;
				}
			}
			if(!ok)
				break;
			conn.setCurrentSchema(event_id);
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
	    AppDbConfig cfg;
		cfg.setEventConfig(new_params.eventConfig);
		for(int i=0; i<stage_count; i++) {
		    StageConfig stcfg;
		    stcfg.startDateTime = new_params.stageStarts.value(i);
		    cfg.setStageConfig(i+1, stcfg);
		}
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
	event_w->setEventId(eventDbName());
	event_w->setEventIdEditable(false);
	EventDialogWidget::Params params;
	params.eventConfig = eventConfig();
	for(int i=0; i<params.eventConfig.stageCount; i++) {
		params.stageStarts << stageStartDateTime(i+1);
	}
	event_w->loadParams(params);
	dlg.setCentralWidget(event_w);
	if(!dlg.exec())
		return;

	auto new_params = event_w->saveParams();
	appDbConfig().setEventConfig(new_params.eventConfig);
	for(int i=0; i<new_params.eventConfig.stageCount; i++) {
		auto stc = appDbConfig().stageConfig(i+1);
		stc.startDateTime = new_params.stageStarts.value(i);
		appDbConfig().setStageConfig(i+1, stc);
	}

}

bool EventPlugin::closeEvent()
{
	qfLogFuncFrame();
	m_classNameCache.clear();
	setEventDbName(QString());
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
		// Loop so that deleting an event re-shows the dialog with a refreshed list
		while(true) {
			auto event_infos = loadEventInfoList(connection_type, db_event_names);
			OpenEventDialog dlg(event_infos, dbVersion(), db_event_names, fwk);
			if(dlg.exec() != QDialog::Accepted) {
				ok = false;
				break;
			}
			if(dlg.selectedAction() == OpenEventDialog::RowAction::Delete) {
				deleteEvent(dlg.selectedEventId());
				db_event_names = (connection_type == ConnectionType::SingleFile)
					? existingFileEventNames(connection_settings.singleWorkingDir())
					: existingSqlEventNames();
				if(db_event_names.isEmpty()) { ok = false; break; }
				continue;
			}
			if(dlg.selectedAction() == OpenEventDialog::RowAction::Convert) {
				const QString from_event = dlg.selectedEventId();
				const QString to_event   = dlg.convertedEventId();
				const bool converted = (connection_type == ConnectionType::SingleFile)
					? importEventFromFile(eventNameToFileName(from_event), to_event)
					: convertSqlEvent(from_event, to_event);
				if(converted)
					return openEvent(to_event);
				m_actOpenEvent->setEnabled(!db_event_names.isEmpty());
				return false;
			}
			// RowAction::Open
			event_name = dlg.selectedEventId();
			break;
		}
	}
	if(!eventDbName().isEmpty() && db_event_names.contains(eventDbName()) && !ok) {
		// dialog canceled and event is already open => no change
		return true;
	}
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
		m_appDbConfig = AppDbConfig();
		m_appDbConfig.load();
		if(m_appDbConfig.dbVersion() < dbVersion()) {
			qfd::MessageBox::showError(fwk, tr("Event data version (%1) is too low, minimal version is (%2).\nUse: File --> Import --> Event (*.qbe) to convert event to current version.")
									   .arg(qf::core::Utils::intToVersionString(m_appDbConfig.dbVersion()))
									   .arg(qf::core::Utils::intToVersionString(dbVersion())));
			closeEvent();
			ok = false;
		}
		else if(m_appDbConfig.dbVersion() > dbVersion()) {
			qfd::MessageBox::showError(fwk, tr("Event was created in more recent QuickEvent version (%1) and the application might not work as expected. Download latest QuickEvent is strongly recommended.")
									   .arg(qf::core::Utils::intToVersionString(m_appDbConfig.dbVersion())));
		}
	}
	if(ok) {
		connection_settings.setEventName(event_name);
		setEventDbName(event_name);
		//emit reloadDataRequest();
	}

	m_actOpenEvent->setEnabled(ok || !db_event_names.isEmpty());
	m_actEditEvent->setEnabled(ok);
	m_actExportEvent_qbe->setEnabled(ok);
	setEventOpen(ok);
	emit currentStageIdChanged(currentStageId());
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
			QStringList create_script = db_schema->loadCreateDbSqlScript(create_options);
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

void EventPlugin::deleteEvent(const QString &event_name)
{
	qff::MainWindow *fwk = qff::MainWindow::frameWork();
	if(event_name == eventDbName())
		closeEvent();
	if(connectionType() == ConnectionType::SingleFile) {
		const QString fn = eventNameToFileName(event_name);
		if(!QFile::remove(fn))
			qfd::MessageBox::showError(fwk, tr("Cannot delete event file: %1").arg(fn));
	}
	else {
		qfs::Connection conn(QSqlDatabase::database());
		qfs::Query q(conn);
		if(!q.exec(QStringLiteral("DROP SCHEMA \"%1\" CASCADE").arg(event_name)))
			qfd::MessageBox::showError(fwk, tr("Cannot delete event schema '%1': %2")
			                           .arg(event_name, q.lastErrorText()));
	}
}
namespace {
void cloneDbConnection(qfs::Connection &dst, const qfs::Connection &src)
{
	dst.setHostName(src.hostName());
	dst.setPort(src.port());
	dst.setUserName(src.userName());
	dst.setPassword(src.password());
	dst.setDatabaseName(src.databaseName());
}
}
bool EventPlugin::importEventFromFile(const QString &src_file, const QString &dest_event_name)
{
	qfLogFuncFrame();
	qff::MainWindow *fwk = qff::MainWindow::frameWork();
	QString err_str;
	const QString import_connection_name = QStringLiteral("qe_import_connection");
	const QString export_connection_name = QStringLiteral("qe_export_connection");
	do {
		qfs::Connection current_conn = qfs::Connection::forName();

		qfs::Connection imp_conn(QSqlDatabase::addDatabase("QSQLITE", import_connection_name));
		imp_conn.setDatabaseName(src_file);
		qfInfo() << "Opening import database file" << src_file;
		if(!imp_conn.open()) {
			err_str = tr("Open Database Error: %1").arg(imp_conn.errorString());
			break;
		}

		qfs::Connection exp_conn(QSqlDatabase::addDatabase(current_conn.driverName(), export_connection_name));
		if(connectionType() == ConnectionType::SingleFile)
			exp_conn.setDatabaseName(eventNameToFileName(dest_event_name));
		else
			cloneDbConnection(exp_conn, current_conn);
		qfInfo() << "Opening export database:" << exp_conn.databaseName();
		if(!exp_conn.open()) {
			err_str = tr("Open Database Error: %1").arg(exp_conn.errorString());
			break;
		}

		err_str = copyEventSchema(imp_conn, exp_conn, dest_event_name);
	} while(false);
	QSqlDatabase::removeDatabase(import_connection_name);
	QSqlDatabase::removeDatabase(export_connection_name);
	fwk->hideProgress();
	if(!err_str.isEmpty()) {
		qfd::MessageBox::showError(fwk, err_str);
		return false;
	}
	return true;
}

bool EventPlugin::convertSqlEvent(const QString &from_event, const QString &to_event)
{
	qfLogFuncFrame();
	qff::MainWindow *fwk = qff::MainWindow::frameWork();
	QString err_str;
	const QString import_connection_name = QStringLiteral("qe_import_connection");
	const QString export_connection_name = QStringLiteral("qe_export_connection");
	do {
		qfs::Connection current_conn = qfs::Connection::forName();

		qfs::Connection imp_conn(QSqlDatabase::addDatabase(current_conn.driverName(), import_connection_name));
		cloneDbConnection(imp_conn, current_conn);
		qfInfo() << "Opening import schema" << from_event;
		if(!imp_conn.open()) {
			err_str = tr("Open Database Error: %1").arg(imp_conn.errorString());
			break;
		}
		imp_conn.setCurrentSchema(from_event);

		qfs::Connection exp_conn(QSqlDatabase::addDatabase(current_conn.driverName(), export_connection_name));
		cloneDbConnection(exp_conn, current_conn);
		qfInfo() << "Opening export schema" << to_event;
		if(!exp_conn.open()) {
			err_str = tr("Open Database Error: %1").arg(exp_conn.errorString());
			break;
		}

		err_str = copyEventSchema(imp_conn, exp_conn, to_event);
	} while(false);
	QSqlDatabase::removeDatabase(import_connection_name);
	QSqlDatabase::removeDatabase(export_connection_name);
	fwk->hideProgress();
	if(!err_str.isEmpty()) {
		qfd::MessageBox::showError(fwk, err_str);
		return false;
	}
	return true;
}

QString EventPlugin::copyEventSchema(qfs::Connection &imp_conn, qfs::Connection &exp_conn,
									 const QString &dest_schema_name)
{
	qff::MainWindow *fwk = qff::MainWindow::frameWork();
	QString err_str;
	qfs::Transaction transaction(exp_conn);
	DbSchema *db_schema = dbSchema();
	auto tables = db_schema->tables();
	int step_cnt = tables.count() + 1;
	int step_no = 0;
	fwk->showProgress(tr("Creating database"), ++step_no, step_cnt);
	do {
		DbSchema::CreateDbSqlScriptOptions create_options;
		create_options.setDriverName(exp_conn.driverName());
		create_options.setSchemaName(dest_schema_name);
		QStringList create_script = db_schema->loadCreateDbSqlScript(create_options);
		qfs::Query ex_q(exp_conn);
		if(!run_sql_script(ex_q, create_script)) {
			err_str = tr("Create Database Error: %1").arg(ex_q.lastError().text());
			break;
		}
		exp_conn.setCurrentSchema(dest_schema_name);
		for(QObject *table : tables) {
			const QString table_name = table->property("name").toString();
			qfDebug() << "Copying table" << table_name;
			fwk->showProgress(tr("Copying table %1").arg(table_name), ++step_no, step_cnt);
			QSqlRecord rec = db_schema->sqlRecord(table, true);
			err_str = copy_sql_table(table_name, rec, imp_conn, exp_conn);
			if(!err_str.isEmpty())
				break;
		}
		if(!err_str.isEmpty())
			break;
		repairStageStarts(imp_conn, exp_conn);
		transaction.commit();
	} while(false);
	return err_str;
}

void EventPlugin::importEvent_qbe()
{
	qfLogFuncFrame();
	qff::MainWindow *fwk = qff::MainWindow::frameWork();
	QString ext = ".qbe";
	QString fn = qf::gui::dialogs::FileDialog::getOpenFileName(fwk, tr("Import as Quick Event"), QString(), tr("Quick Event files *%1 (*%1)").arg(ext));
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
	if(importEventFromFile(fn, event_name)) {
		if(qfd::MessageBox::askYesNo(fwk, tr("Open imported event '%1'?").arg(event_name), false)) {
			openEvent(event_name);
		}
	}
}

void EventPlugin::onServiceDockVisibleChanged(bool on)
{
	if(on && !m_servicesDockWidget->widget()) {
		auto *rw = new services::ServicesWidget();
		rw->setEnabled(isEventOpen());
		connect(this, &EventPlugin::eventOpenChanged, rw, [rw](bool is_open) {
			rw->setEnabled(is_open);
		});
		m_servicesDockWidget->setWidget(rw);
		rw->reload();
	}
}

void EventPlugin::onDbEventNotify(const QString &domain, int connection_id, const QVariant &data)
{
	Q_UNUSED(connection_id)
	qfLogFuncFrame() << "domain:" << domain << "payload:" << data;
	if(domain == QLatin1String(Event::EventPlugin::DBEVENT_REGISTRATIONS_IMPORTED)) {
		reloadRegistrationsModel();
	}
}

void EventPlugin::onRecChng(const qf::core::sql::QxRecChng &recchng)
{
	if (recchng.issuer != qf::gui::framework::Application::uuidString()) {
		return;
	}
	emitDbEvent(DBEVENT_QX_RECCHNG, recchng.toVariantMap(), false);
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
