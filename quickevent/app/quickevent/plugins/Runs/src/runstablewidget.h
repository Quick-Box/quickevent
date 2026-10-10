#pragma once

#include <QWidget>

class RunsTableModel;
class RunsTableItemDelegate;
class CourseItemDelegate;

namespace qf::gui { class TableView; }

namespace Ui {
class RunsTableWidget;
}

class RunsTableWidget : public QWidget
{
	Q_OBJECT
private:
	typedef QWidget Super;
public:
	explicit RunsTableWidget(QWidget *parent = nullptr);
	~RunsTableWidget() Q_DECL_OVERRIDE;

	void clear();
	void reload(int stage_id, int class_id = 0, bool show_offrace = false, const QString &sort_column = QString(), int select_competitor_id = 0, bool only_on_track = false);
	void reload();

	RunsTableModel* runsModel() {return m_runsModel;}
	qf::gui::TableView* tableView();
	void setColumnsHidden(const QStringList &hidden_field_names);
	void setColumnsOrder(const QStringList &ordered_field_names);

	Q_SIGNAL void editCompetitorRequest(int competitor_id, int mode);
private:
	QMap<int, QString> definedCourses();

	void updateStartTimeHighlight() const;
	void onCustomContextMenuRequest(const QPoint &pos);
	void onTableViewSqlException(const QString &what, const QString &where, const QString &stack_trace);
	void onBadTableDataInput(const QString &message);
private:
	Ui::RunsTableWidget *ui;
	RunsTableModel *m_runsModel;
	RunsTableItemDelegate *m_runsTableItemDelegate;
	CourseItemDelegate *m_courseItemDelegate = nullptr;
};
