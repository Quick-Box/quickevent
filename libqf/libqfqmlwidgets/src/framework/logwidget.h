#ifndef QF_QMLWIDGETS_FRAMEWORK_LOGWIDGET_H
#define QF_QMLWIDGETS_FRAMEWORK_LOGWIDGET_H

#include "dockablewidget.h"

#include <qf/core/logentrymap.h>

#include <QTableView>

class QAbstractButton;
class QHBoxLayout;

namespace qf {
namespace core {
class LogEntryMap;
namespace model { class LogTableModel; }
}
}

namespace qf {
namespace qmlwidgets {
namespace framework {

class LogFilterProxyModel;

namespace Ui {
class LogWidget;
}

class QFQMLWIDGETS_DECL_EXPORT LogWidgetTableView : public QTableView
{
	Q_OBJECT
private:
	typedef QTableView Super;
public:
	LogWidgetTableView(QWidget *parent);

	void copy();
	QAction *copySelectionToClipboardAction() const { return m_copySelectionToClipboardAction; }
private:
	QAction *m_copySelectionToClipboardAction;
};

class QFQMLWIDGETS_DECL_EXPORT LogWidget : public qf::qmlwidgets::framework::DockableWidget
{
	Q_OBJECT
private:
	typedef qf::qmlwidgets::framework::DockableWidget Super;
public:
	explicit LogWidget(QWidget *parent = nullptr);
	~LogWidget();

	Q_SLOT void addLog(NecroLog::Level severity, const QString& category, const QString &file, int line, const QString& msg, const QDateTime& time_stamp, const QString &function, const QVariant &user_data = QVariant());
	Q_SLOT void addLogEntry(const qf::core::LogEntryMap &le);
	Q_SLOT void checkScrollToLastEntry();

	void clear();
	virtual void setLogTableModel(qf::core::model::LogTableModel *m);
	qf::core::model::LogTableModel* logTableModel();

	Q_SIGNAL void severityTresholdChanged(NecroLog::Level lvl);
	void setSeverityTreshold(NecroLog::Level lvl);
	NecroLog::Level severityTreshold() const;
protected:
	QAbstractButton* clearButton();
	QAbstractButton* tableMenuButton();
	QTableView* tableView() const;

	void clearCategoryActions();
	virtual void addCategoryActions(const QString &caption, const QString &id, NecroLog::Level level = NecroLog::Level::Invalid);
	QMap<QString, NecroLog::Level> selectedLogCategories() const;
	virtual void registerLogCategories();

	virtual void onDockWidgetVisibleChanged(bool visible);
	void onVerticalScrollBarValueChanged();
	void onSeverityTresholdIndexChanged(int index);
private:
	void filterStringChanged(const QString &filter_string);
	void on_btClearLog_clicked();
	void on_btResizeColumns_clicked();

	bool isAutoScroll();
protected:
	qf::core::model::LogTableModel* m_logTableModel = nullptr;
	LogFilterProxyModel* m_filterModel = nullptr;
	QHBoxLayout *horizontalLayout();
private:
	Ui::LogWidget *ui;
	bool m_logCategoriesRegistered = false;

	QList<QAction*> m_logLevelActions;
	QList<QMenu*> m_loggingCategoriesMenus;
	bool m_columnsResized = false;
};


} // namespace framework
} // namespace qmlwiggets
} // namespace qf
#endif // QF_QMLWIDGETS_FRAMEWORK_LOGWIDGET_H
