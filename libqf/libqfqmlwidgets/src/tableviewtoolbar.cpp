#include "tableviewtoolbar.h"
#include "tableview.h"
#include "action.h"
#include "style.h"

#include <qf/core/log.h>

#include <QComboBox>
#include <QLineEdit>
#include <QLabel>
#include <QKeyEvent>

//namespace qfc = qf::core;
//namespace qfu = qf::core::utils;
using namespace qf::qmlwidgets;

void FilterCombo::keyReleaseEvent(QKeyEvent *ev)
{
	qfLogFuncFrame() << ev->key() << (ev->key() == Qt::Key_Escape);
	if(ev->key() != Qt::Key_Escape) {
		Super::keyReleaseEvent(ev);
	}
	else {
		emit filterFocusReleased();
	}
}

TableViewToolBar::TableViewToolBar(QWidget *parent) :
	QToolBar(parent)
{
	m_filterCombo = new FilterCombo();
	//m_filterCombo->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Preferred);
	m_filterCombo->setMinimumWidth(fontMetrics().horizontalAdvance('X') * 15);
	m_filterCombo->setEditable(true);
	m_filterCombo->lineEdit()->setClearButtonEnabled(true);
	connect(m_filterCombo, &QComboBox::editTextChanged, this, &TableViewToolBar::emitFilterStringChanged);
#if QT_VERSION_MAJOR >= 6
	connect(m_filterCombo, &QComboBox::activated, this, &TableViewToolBar::emitFilterStringChanged);
#else
	connect(m_filterCombo, QOverload<int>::of(&QComboBox::activated), this, &TableViewToolBar::emitFilterStringChanged);
#endif
}



void TableViewToolBar::setTableView(TableView *table_view)
{
	qfLogFuncFrame() << m_filterCombo;
	if(table_view) {
		m_pendingActions = table_view->toolBarActions();
		/// cannot add actions here from QML context because of bug in Qt5.3.1
		QMetaObject::invokeMethod(this, "addPendingActions", Qt::QueuedConnection);
		connect(this, &TableViewToolBar::filterStringChanged, table_view, &TableView::filterByString);
		connect(table_view, &TableView::filterDialogRequest, this, &TableViewToolBar::onFilterDialogRequest);
		if(m_filterCombo) {
			connect(m_filterCombo, &FilterCombo::filterFocusReleased, table_view, [table_view]() { table_view->setFocus(); });
		}
	}
}

void TableViewToolBar::addPendingActions()
{
	QList<QAction*> lst;
	Q_FOREACH(auto a, m_pendingActions)
		lst << a;
	addActions(lst);
	auto *lbl = new QLabel(tr("Filter"));
	auto *style = Style::instance();
	QPixmap px = style->pixmap("find");
	lbl->setPixmap(px);
	addWidget(lbl);
	addWidget(m_filterCombo);
}

void TableViewToolBar::emitFilterStringChanged()
{
	auto s = m_filterCombo->currentText();
	qfLogFuncFrame() << s;
	emit filterStringChanged(s);
	m_filterCombo->setFocus();
}

void TableViewToolBar::onFilterDialogRequest()
{
	m_filterCombo->lineEdit()->selectAll();
	m_filterCombo->setFocus();
}

