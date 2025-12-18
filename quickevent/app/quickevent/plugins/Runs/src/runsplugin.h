#ifndef RUNS_RUNSPLUGIN_H
#define RUNS_RUNSPLUGIN_H

#include <quickevent/core/og/timems.h>
#include <quickevent/core/coursedef.h>
#include <quickevent/gui/reportoptionsdialog.h>

#include <qf/gui/framework/plugin.h>

#include <qf/core/utils.h>
#include <qf/core/utils/table.h>

namespace qf {
	namespace core {
		namespace utils {
			class Table;
			class TreeTable;
		}
	}
	namespace gui {
		class Action;
		namespace framework {
			class PartWidget;
			class DockWidget;
		}
	}
}

namespace qf::core::sql { class QueryBuilder; }

namespace Runs {

class RunsPlugin : public qf::gui::framework::Plugin
{
	Q_OBJECT
	//Q_PROPERTY(qf::gui::framework::PartWidget* partWidget READ partWidget FINAL)
	Q_PROPERTY(int selectedStageId READ selectedStageId WRITE setSelectedStageId NOTIFY selectedStageIdChanged)
private:
	typedef qf::gui::framework::Plugin Super;
public:
	static constexpr int UNREAL_TIME_MSEC = quickevent::core::og::TimeMs::UNREAL_TIME_MSEC;
public:
	RunsPlugin(QObject *parent = nullptr);
	~RunsPlugin() Q_DECL_OVERRIDE;

	QF_PROPERTY_IMPL2(int, s, S, electedStageId, 1)

	//qf::gui::framework::PartWidget *partWidget() {return m_partWidget;}

	const qf::core::utils::Table& runnersTable(int stage_id);
	Q_SLOT void clearRunnersTableCache();

	Q_INVOKABLE int courseForRun(int run_id);
	int courseForRelay(int relay_number, int leg);
	quickevent::core::CourseDef courseCodesForRunId(int run_id);
	quickevent::core::CourseDef courseForCourseId(int course_id);

	Q_INVOKABLE int cardForRun(int run_id);
	qf::core::utils::TreeTable currentStageResultsTable(const QString &class_filter = QString(), int max_competitors_in_class = 0, bool exclude_disq = false);
	Q_INVOKABLE QVariant currentStageResultsTableData(const QString &class_filter, int max_competitors_in_class = 0, bool exclude_disq = false);

	Q_INVOKABLE QVariant stageResultsTableData(int stage_id, const QString &class_filter, int max_competitors_in_class = 0, bool exclude_disq = false);
	qf::core::utils::TreeTable stageResultsTable(int stage_id, const QString &class_filter = QString(), int max_competitors_in_class = 0, bool exclude_disq = false, bool add_laps = false);

	qf::core::utils::Table nstagesClassResultsTable(int stages_count, int class_id, int places = -1, bool exclude_disq = true);
	qf::core::utils::TreeTable nstagesResultsTable(const QString &class_filter, int stages_count, int places = -1, bool exclude_disq = true);
	//Q_INVOKABLE QVariant nstagesResultsTableData(int stages_count, int places = -1, bool exclude_disq = true);
	Q_INVOKABLE void showRunsTable(int stage_id, int class_id, bool show_offrace, const QString &sort_column = QString(), int select_competitor_id = 0);

	Q_INVOKABLE QWidget* createReportOptionsDialog(QWidget *parent = nullptr);
	Q_INVOKABLE QWidget* createNStagesReportOptionsDialog(QWidget *parent = nullptr);

	Q_INVOKABLE bool reloadTimesFromCard(int run_id);

	Q_INVOKABLE QVariantMap printAwardsOptionsWithDialog(const QVariantMap &opts);

        bool exportStartListStageIofXml30(int stage_id, const QString &file_name, bool with_vacants);
	bool exportStartListCurrentStageCsvSime(const QString &file_name, bool bibs, QString sql_where);
	bool exportStartListCurrentStageTvGraphics(const QString &file_name);

	QVariantList qxExportRunsCsvJson(int stage_id);
	// QString qxExportRunsCsv(int stage_id);

	//bool exportResultsHtmlStage(int stage_id, const QString &file_name);
	Q_INVOKABLE bool exportResultsIofXml30Stage(int stage_id, const QString &file_name);
	Q_INVOKABLE bool exportResultsCsosStage(int stage_id, const QString &file_name);
	Q_INVOKABLE bool exportResultsCsosOverall(int stage_count, const QString &file_name);

	qf::core::sql::QueryBuilder runsQuery(int stage_id, int class_id = 0, bool show_offrace = false);
	QVariantMap runsRecord(int run_id);
	void setRunsRecord(int run_id, const QVariant &rec);

	qf::core::sql::QueryBuilder startListQuery();
	QVariantMap startListRecord(int run_id);

	qf::core::utils::TreeTable startListClassesTable(const QString &where_expr, const bool insert_vacants, const quickevent::gui::ReportOptionsDialog::StartTimeFormat start_time_format);
	qf::core::utils::TreeTable startListClubsTable(const quickevent::gui::ReportOptionsDialog::StartTimeFormat start_time_format, const quickevent::gui::ReportOptionsDialog::StartlistOrderFirstBy order_first_by);
	qf::core::utils::TreeTable startListStartersTable(const QString &where_expr);
	qf::core::utils::TreeTable startListClassesNStagesTable(const int stages_count, const QString &where_expr, const quickevent::gui::ReportOptionsDialog::StartTimeFormat start_time_format);
	qf::core::utils::TreeTable startListClubsNStagesTable(const int stages_count, const quickevent::gui::ReportOptionsDialog::StartTimeFormat start_time_format);

	void editCompetitorOnPunch(int siid) { emit editCompetitorOnPunchRequest(siid); }
	Q_SIGNAL void editCompetitorOnPunchRequest(int siid);
public:
	void report_startListClasses();
	void report_startListClubs();
	void report_startListStarters();
	void report_startListClassesNStages();
	void report_startListClubsNStages();

	void report_resultsClasses();
	void report_resultsForSpeaker();
	void report_resultsAwards();
	void report_resultsNStages();
	void report_resultsNStagesSpeaker();
	void report_nStagesAwards();

	void export_startListClassesHtml();
	void export_startListClubsHtml();

	QString export_resultsHtmlStage(bool with_laps = false);
	void export_resultsHtmlStageWithLaps();
	void export_resultsHtmlNStages();
        QString startListStageIofXml30(int stage_id, bool with_vacants);
	QString resultsIofXml30Stage(int stage_id);
	int competitorForRun(int run_id);
	int runForCompetitorStage(int competitor_id, int stage_id);
private:
	Q_SLOT void onInstalled();

	qf::core::utils::TreeTable addLapsToStageResultsTable(int course_id, const qf::core::utils::TreeTable &class_results);
	void exportResultsHtmlStageWithLaps(const QString &laps_file_name, const qf::core::utils::TreeTable &tt);

	int courseForRun_Classic(int run_id);
	int courseForRun_Relays(int run_id);
	QString getClubAbbrFromName(QString name);

	void writeCSOSHeader(QTextStream &ts) const;

	void addStartTimeTextToClass(qf::core::utils::TreeTable &tt2, const qint64 start00_epoch_sec, const quickevent::gui::ReportOptionsDialog::StartTimeFormat start_time_format);
	void addStartTimeTextToClass(qf::core::utils::TreeTable &tt2, const int stages_count, const QVector<qint64> &start00_epoch_sec, const quickevent::gui::ReportOptionsDialog::StartTimeFormat start_time_format);
private:
	qf::gui::framework::PartWidget *m_partWidget = nullptr;
	qf::core::utils::Table m_runnersTableCache;
	int m_runnersTableCacheStageId = 0;
	qf::gui::framework::DockWidget *m_eventStatisticsDockWidget = nullptr;
	qf::gui::framework::DockWidget *m_qxLateRegistrationsDockWidget = nullptr;
};

}

#endif // RUNS_RUNSPLUGIN_H
