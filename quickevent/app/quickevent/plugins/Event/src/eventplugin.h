#ifndef EVENTPLUGIN_H
#define EVENTPLUGIN_H

#include "eventconfig.h"
#include "stage.h"

#include <qf/gui/framework/plugin.h>

#include <qf/core/utils.h>
#include <qf/core/utils/table.h>

#include <QVariantMap>
#include <QSqlDriver>

namespace qf::core::sql { class Query; class Connection; }
namespace qf::gui { class Action; }
namespace qf::gui::framework { class DockWidget; }
namespace qf::gui::model { class SqlTableModel; }

class QComboBox;
class DbSchema;

namespace Event {


static constexpr auto START_LIST_IOFXML3_FILE = "startlist-iof3.xml";
static constexpr auto RESULTS_IOFXML3_FILE = "results-iof3.xml";

class EventPlugin : public qf::gui::framework::Plugin
{
	Q_OBJECT
	Q_PROPERTY(QObject* eventConfig READ eventConfig)
	Q_PROPERTY(int currentStageId READ currentStageId NOTIFY currentStageIdChanged)
	Q_PROPERTY(int stageCount READ stageCount)
	Q_PROPERTY(QString eventName READ eventName WRITE setEventName NOTIFY eventNameChanged)
	Q_PROPERTY(bool eventOpen READ isEventOpen WRITE setEventOpen NOTIFY eventOpenChanged)
	Q_PROPERTY(bool sqlServerConnected READ isSqlServerConnected NOTIFY sqlServerConnectedChanged)
private:
	using Super = qf::gui::framework::Plugin;
public:
	enum class ConnectionType : int {SqlServer = 0, SingleFile};
public:
	EventPlugin(QObject *parent = nullptr);

	QF_PROPERTY_BOOL_IMPL(e, E, ventOpen)
	QF_PROPERTY_IMPL(QString, e, E, ventName)

	/// strange is that 'quickboxDbEvent' just doesn't work without any error
	/// from psql doc: Commonly, the channel name is the same as the name of some table in the database
	/// I guess that channel name cannot contain capital letters to work
	static constexpr auto DBEVENT_NOTIFY_NAME = "quickbox_db_event";

	static constexpr auto DBEVENT_COMPETITOR_COUNTS_CHANGED = "competitorCountsChanged";
	static constexpr auto DBEVENT_CARD_READ = "cardRead";
	static constexpr auto DBEVENT_COMPETITOR_EDITED = "competitorEdited";
	static constexpr auto DBEVENT_COMPETITOR_ADDED = "competitorAdded";
	static constexpr auto DBEVENT_COMPETITOR_DELETED = "competitorDeleted";
	static constexpr auto DBEVENT_RUN_CHANGED = "runChanged";
	static constexpr auto DBEVENT_CARD_PROCESSED_AND_ASSIGNED = "cardProcessedAndAssigned";
	static constexpr auto DBEVENT_PUNCH_RECEIVED = "punchReceived";
	static constexpr auto DBEVENT_REGISTRATIONS_IMPORTED = "registrationsImported";
	static constexpr auto DBEVENT_STAGE_START_CHANGED = "stageStartChanged";
	static constexpr auto DBEVENT_QX_CHANGE_RECEIVED = "qxChangeReceived";

	Q_INVOKABLE void initEventConfig();
	Event::EventConfig* eventConfig(bool reload = false);
	int stageCount();

	Q_SLOT void setCurrentStageId(int stage_id);
	int currentStageId() const;
	Q_SIGNAL void currentStageIdChanged(int current_stage);

	Q_INVOKABLE int stageIdForRun(int run_id);

	Q_INVOKABLE int stageStartMsec(int stage_id);
	Q_INVOKABLE QDate stageStartDate(int stage_id);
	Q_INVOKABLE QTime stageStartTime(int stage_id);
	Q_INVOKABLE QDateTime stageStartDateTime(int stage_id);
	//Q_INVOKABLE int currentStageStartMsec();
	int msecToStageStartAM(int si_am_time_sec, int msec = 0, int stage_id = 0);

	StageData stageData(int stage_id);
	void setStageData(int stage_id, const StageData &data);
	Q_SLOT void clearStageDataCache();

	Q_SLOT bool createEvent(const QString &event_name = QString(), const QVariantMap &event_params = QVariantMap());
	Q_SLOT void editEvent();
	Q_SLOT bool closeEvent();
	Q_SLOT bool openEvent(const QString &event_name = QString());
	Q_SLOT void exportEvent_qbe();
	Q_SLOT void importEvent_qbe();

	void emitReloadDataRequest() { emit reloadDataRequest(); }
	Q_SIGNAL void reloadDataRequest();

	bool isSqlServerConnected() const { return m_sqlServerConnected; }
	Q_SIGNAL void sqlServerConnectedChanged(bool is_open);

	Q_INVOKABLE void emitDbEvent(const QString &domain, const QVariant &data = QVariant(), bool loopback = true);
	/// emitted only if loopback is not set
	Q_SIGNAL void dbEventNotify(const QString &domain, int connection_id, const QVariant &payload);

	Q_INVOKABLE QString sqlDriverName();

	Q_INVOKABLE QString classNameById(int class_id);

	QString shvApiEventId() const;
	static QString createApiKey(int length);

	DbSchema* dbSchema();
	static int dbVersion();
	static QString dbVersionString();

	Q_SLOT void onInstalled();

	qf::gui::model::SqlTableModel* registrationsModel();
	const qf::core::utils::Table& registrationsTable();

public:
	ConnectionType connectionType() const;
	bool isSingleUser() const;
private:
	void setSqlServerConnected(bool ok);

	QStringList existingSqlEventNames() const;
	QStringList existingFileEventNames(const QString &dir = QString()) const;

	Q_SLOT void onEventOpened();
	Q_SLOT void connectToSqlServer();
	Q_SLOT void loadCurrentStageId();
	Q_SLOT void saveCurrentStageId(int current_stage);
	Q_SLOT void editStage();
	Q_SLOT void onDbEvent(const QString & name, QSqlDriver::NotificationSource source, const QVariant & payload);

	void onDbEventNotify(const QString &domain, int connection_id, const QVariant &data);

	void onRegistrationsDockVisibleChanged(bool on = true);

	void updateWindowTitle() const;
	void reloadRegistrationsModel();

	//bool runSqlScript(qf::core::sql::Query &q, const QStringList &sql_lines);
	void repairStageStarts(const qf::core::sql::Connection &from_conn, const qf::core::sql::Connection &to_conn);

	void onServiceDockVisibleChanged(bool on = true);
private:
	qf::gui::Action *m_actConnectDb = nullptr;
	qf::gui::Action *m_actEvent = nullptr;
	qf::gui::Action *m_actImport = nullptr;
	qf::gui::Action *m_actExport = nullptr;
	qf::gui::Action *m_actCreateEvent = nullptr;
	qf::gui::Action *m_actOpenEvent = nullptr;
	qf::gui::Action *m_actEditEvent = nullptr;
	qf::gui::Action *m_actExportEvent_qbe = nullptr;
	qf::gui::Action *m_actImportEvent_qbe = nullptr;
	qf::gui::Action *m_actEditStage = nullptr;
	Event::EventConfig *m_eventConfig = nullptr;
	bool m_sqlServerConnected = false;
	QComboBox *m_cbxStage = nullptr;
	int m_currentStageId = 0;
	QMap<int, StageData> m_stageCache;
	QMap<int, QString> m_classNameCache;

	qf::gui::framework::DockWidget *m_servicesDockWidget = nullptr;
	qf::gui::framework::DockWidget *m_registrationsDockWidget = nullptr;

	qf::gui::model::SqlTableModel *m_registrationsModel = nullptr;
	qf::core::utils::Table m_registrationsTable;
};

}

#endif // EVENTPLUGIN_H
