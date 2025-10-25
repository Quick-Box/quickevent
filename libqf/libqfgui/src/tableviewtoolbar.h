#ifndef QF_GUI_TABLEVIEWTOOLBAR_H
#define QF_GUI_TABLEVIEWTOOLBAR_H

#include "guiglobal.h"

#include <QComboBox>
#include <QToolBar>

namespace qf {
namespace gui {

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

class QFGUI_DECL_EXPORT TableViewToolBar : public QToolBar
{
	Q_OBJECT

	using Super = QToolBar;

	Q_PROPERTY(qf::gui::TableView* tableView READ tableView WRITE setTableView FINAL)
public:
	explicit TableViewToolBar(QWidget *parent = nullptr);
	~TableViewToolBar() Q_DECL_OVERRIDE = default;
public:
	void setTableView(TableView *table_view);
	qf::gui::TableView* tableView() const { return m_tableView; }
signals:
	void filterStringChanged(const QString &s);
private:
	void emitFilterStringChanged();
	void onFilterDialogRequest();
private:
	qf::gui::TableView* m_tableView = nullptr;
	FilterCombo *m_filterCombo = nullptr;
};

}}

#endif // QF_GUI_TABLEVIEWTOOLBAR_H
