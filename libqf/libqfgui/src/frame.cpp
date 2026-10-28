#include "frame.h"
//#include "LayoutTypeProperties.h"
#include "layouttypeproperties.h"

#include <qf/core/log.h>
#include <qf/core/assert.h>

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QFormLayout>
#include <QLabel>

using namespace qf::gui;

static const int DefaultLayoutMargin = 1;

Frame::Frame(QWidget *parent) :
	Super(parent), m_layoutType(LayoutInvalid), m_layoutTypeProperties(nullptr),
	m_currentLayoutRow(), m_currentLayoutColumn()
{
}

Frame::~Frame()
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
		
			delete old_ly;
		
		m_layoutType = ly_type;
		Q_FOREACH(auto w, m_childWidgets) {
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

void Frame::setVisible(bool b)
{
	if(b != isVisible()) {
		Super::setVisible(b);
		emit visibleChanged(b);
	}
}

void Frame::add(QWidget *widget)
{
	if(widget) {
		//qDebug() << "adding widget" << widget << widget->parent();
		/// widget cannot be simply reparented
		/// NULL parent should be set first
		widget->setParent(nullptr);
		widget->setParent(this);
		m_childWidgets << widget;
		//Super::layout()->addWidget(widget);
		widget->show();
		//m_layout->addWidget(new QLabel("ahoj", this));
		addToLayout(widget);
	}
}

void Frame::setLayoutTypeProperties(LayoutTypeProperties *props)
{
	m_layoutTypeProperties = props;
}

void Frame::addToLayout(QWidget *widget)
{
	qfLogFuncFrame();
	QF_ASSERT(widget != nullptr, "Widget is NULL", return);
	if(!layout()) {
		createLayout(layoutType());
		qfDebug() << "\tnew layout:" << layout() << this;
	}
	qfDebug() << "\tadding:" << widget << "to layout:" << layout() << this;
	{
		auto *ly = qobject_cast<QGridLayout*>(layout());
		if(ly) {
			LayoutTypeProperties *props = layoutTypeProperties();
			LayoutTypeProperties::Flow flow = LayoutTypeProperties::LeftToRight;
			int cnt = -1;
			if(!props) {
				qfWarning() << this << "missing gridLayoutProperties property for GridLayout type";
			}
			else {
				flow = props->flow();
				cnt = (flow == LayoutTypeProperties::LeftToRight)? props->columns(): props->rows();
			}
			if(cnt <= 0)
				cnt = 2;
			if(flow == LayoutTypeProperties::LeftToRight) {
				ly->addWidget(widget, m_currentLayoutRow, m_currentLayoutColumn);
				m_currentLayoutColumn++;
				if(m_currentLayoutColumn >= cnt) {
					m_currentLayoutColumn = 0;
					m_currentLayoutRow++;
				}
			}
			else {
				ly->addWidget(widget, m_currentLayoutRow, m_currentLayoutColumn);
				m_currentLayoutRow++;
				if(m_currentLayoutRow >= cnt) {
					m_currentLayoutRow = 0;
					m_currentLayoutColumn++;
				}
			}
			return;
		}
	}
	{
		auto *ly = qobject_cast<QFormLayout*>(layout());
		if(ly) {
			ly->addWidget(widget);
			return;
		}
	}
	{
		auto *ly = qobject_cast<QBoxLayout*>(layout());
		if(ly) {
			ly->addWidget(widget);
			return;
		}
	}
}

void Frame::createLayout(LayoutType layout_type)
{
	QF_ASSERT(layout() == nullptr, "Form has layout already", return);
	m_currentLayoutColumn = m_currentLayoutRow = 0;
	QLayout *new_ly;
	QBoxLayout *new_box_ly = nullptr;
	switch(layout_type) {
	case LayoutGrid:
		new_ly = new QGridLayout();
		break;
	case LayoutForm:
		new_ly = new QFormLayout();
		break;
	case LayoutHorizontal:
		new_ly = new_box_ly = new QHBoxLayout();
		break;
	default:
		new_ly = new_box_ly = new QVBoxLayout();
		break;
	}
	new_ly->setContentsMargins(DefaultLayoutMargin, DefaultLayoutMargin, DefaultLayoutMargin, DefaultLayoutMargin);
	//setFrameShape(QFrame::Box);
	if(new_box_ly) {
		LayoutTypeProperties *props = layoutTypeProperties();
		if(props && props->spacing() >= 0)
			new_box_ly->setSpacing(props->spacing());
	}
	setLayout(new_ly);
}

