#ifndef QF_QMLWIDGETS_TABLEVIEWTOOLBAR_H
#define QF_QMLWIDGETS_TABLEVIEWTOOLBAR_H

#include "qmlwidgetsglobal.h"

#include <QComboBox>
#include <QToolBar>

namespace qf {
namespace qmlwidgets {

class TableView;
class Action;

class FilterCombo : public QComboBox
{
	Q_OBJECT
private:
	typedef QComboBox Super;
public:
	FilterCombo(QWidget *parent = nullptr) : QComboBox(parent) {}
	~FilterCombo() Q_DECL_OVERRIDE {}

	Q_SIGNAL void filterFocusReleased();
protected:
	void keyReleaseEvent(QKeyEvent *ev) Q_DECL_OVERRIDE;
};

class QFQMLWIDGETS_DECL_EXPORT TableViewToolBar : public QToolBar
{
	Q_OBJECT

	using Super = QToolBar;

	Q_PROPERTY(qf::qmlwidgets::TableView* tableView READ tableView WRITE setTableView FINAL)
public:
	explicit TableViewToolBar(QWidget *parent = nullptr);
	~TableViewToolBar() Q_DECL_OVERRIDE = default;
public:
	void setTableView(TableView *table_view);
	qf::qmlwidgets::TableView* tableView() const { return m_tableView; }
signals:
	void filterStringChanged(const QString &s);
private:
	Q_SLOT void addPendingActions();
	Q_SLOT void emitFilterStringChanged();
	Q_SLOT void onFilterDialogRequest();
private:
	qf::qmlwidgets::TableView* m_tableView = nullptr;
	QList<Action*> m_pendingActions;
	FilterCombo *m_filterCombo = nullptr;
};

}}

#endif // QF_QMLWIDGETS_TABLEVIEWTOOLBAR_H
