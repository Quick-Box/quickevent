#include "frame.h"
#include "gridlayoutproperties.h"
#include "layoutpropertiesattached.h"

#include <qf/core/log.h>

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QLabel>

#include <QDebug>

using namespace qf::qmlwidgets;

static const int DefaultLayoutMargin = 1;

Frame::Frame(QWidget *parent) :
	Super(parent), m_layoutType(LayoutInvalid), m_gridLayoutProperties(nullptr)
{
}

Frame::LayoutType Frame::layoutType() const
{
	return m_layoutType;
}

void Frame::setLayoutType(Frame::LayoutType ly_type)
{
	qfLogFuncFrame() << ly_type;
	if(ly_type != layoutType()) {
		//QList<QLayoutItem*> layout_items;
		QLayout *old_ly = layout();
		if(old_ly) {
			delete old_ly;
		}
		m_layoutType = ly_type;
		for(auto w : m_childWidgets) {
			addToLayout(w);
		}
		qfDebug() << "new layout:" << layout();
		if(layout()) {
			// visualize changes in layouting
			layout()->activate();
		}
		emit layoutTypeChanged(ly_type);
	}
}

QQmlListProperty<QWidget> Frame::widgets()
{
	return QQmlListProperty<QWidget>(this,0,
                                    Frame::addWidgetFunction,
                                    Frame::countWidgetsFunction,
                                    Frame::widgetAtFunction,
                                    Frame::removeAllWidgetsFunction
                                    );
}

void Frame::addWidgetFunction(QQmlListProperty<QWidget> *list_property, QWidget *value)
{
    if (value) {
        Frame *that = static_cast<Frame*>(list_property->object);
        that->add(value);
    }    
}    

QWidget * Frame::widgetAtFunction(QQmlListProperty<QWidget> *list_property, int index)
{
    Frame *that = static_cast<Frame*>(list_property->object);
    return that->at(index);
}


void Frame::removeAllWidgetsFunction(QQmlListProperty<QWidget> *list_property)
{
    Frame *that = static_cast<Frame*>(list_property->object);
    that->removeAll();
}

int Frame::countWidgetsFunction(QQmlListProperty<QWidget> *list_property)
{
    Frame *that = static_cast<Frame*>(list_property->object);
    return that->count();
}

void Frame::add(QWidget *widget)
{
	if(widget) {
		qDebug() << "adding widget" << widget << widget->parent();
		widget->setParent(0);
		widget->setParent(this);
		m_childWidgets << widget;
		//Super::layout()->addWidget(widget);
		widget->show();
		//m_layout->addWidget(new QLabel("ahoj", this));
		addToLayout(widget);
	}
}

QWidget *Frame::at(int index) const
{
	return m_childWidgets.value(index);
}

void Frame::removeAll()
{
	qDeleteAll(m_childWidgets);
	m_childWidgets.clear();
}

int Frame::count() const
{
	return m_childWidgets.count();
}

void Frame::setGridLayoutProperties(GridLayoutProperties *props)
{
	m_gridLayoutProperties = props;
}

void Frame::addToLayout(QWidget *widget)
{
	qfLogFuncFrame();
	if(!layout()) {
		QLayout *new_ly = createLayout(layoutType());
		qfDebug() << "\tnew layout:" << new_ly << this;
		setLayout(new_ly);
	}
	{
		QGridLayout *ly = qobject_cast<QGridLayout*>(layout());
		if(ly) {
			GridLayoutProperties *props = gridLayoutProperties();
			GridLayoutProperties::Flow flow = GridLayoutProperties::LeftToRight;
			int cnt = -1;
			if(!props) {
				qfWarning() << this << "missing gridLayoutProperties property for GridLayout type";
			}
			else {
				flow = props->flow();
				cnt = (flow == GridLayoutProperties::LeftToRight)? props->columns(): props->rows();
			}
			if(cnt <= 0)
				cnt = 2;
			int row_span = 1, column_span = 1;
			LayoutPropertiesAttached *lpa = qobject_cast<LayoutPropertiesAttached*>(qmlAttachedPropertiesObject<LayoutProperties>(widget, false));
			if(lpa) {
				row_span = lpa->rowSpan();
				column_span = lpa->columnSpan();
			}
			if(flow == GridLayoutProperties::LeftToRight) {
				ly->addWidget(widget, m_currentLayoutRow, m_currentLayoutColumn, 1, column_span);
				m_currentLayoutColumn += column_span;
				if(m_currentLayoutColumn >= cnt) {
					m_currentLayoutColumn = 0;
					m_currentLayoutRow++;
				}
			}
			else {
				ly->addWidget(widget, m_currentLayoutRow, m_currentLayoutColumn, row_span, 1);
				m_currentLayoutRow += row_span;
				if(m_currentLayoutRow >= cnt) {
					m_currentLayoutRow = 0;
					m_currentLayoutColumn++;
				}
			}
			return;
		}
	}
	{
		QBoxLayout *ly = qobject_cast<QBoxLayout*>(layout());
		if(ly) {
			qfDebug() << "\tadding:" << widget << "to layout:" << ly << this;
			ly->addWidget(widget);
			return;
		}
	}
}

QLayout *Frame::createLayout(LayoutType layout_type)
{
	QLayout *ret;
	switch(layout_type) {
	case LayoutGrid:
		m_currentLayoutColumn = m_currentLayoutRow = 0;
		ret = new QGridLayout();
		break;
	case LayoutVertical:
		ret = new QVBoxLayout();
		break;
	default:
		ret = new QHBoxLayout();
		break;
	}
	ret->setMargin(DefaultLayoutMargin);
	setFrameShape(QFrame::Box);
	return ret;
}
