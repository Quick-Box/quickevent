#include "partswitch.h"
#include "partwidget.h"
#include "stackedcentralwidget.h"
#include "mainwindow.h"

#include <qf/core/log.h>
#include <qf/core/assert.h>

#include <QIcon>

using namespace qf::gui::framework;

PartSwitchToolButton::PartSwitchToolButton(QWidget *parent)
	: Super(parent), m_partIndex()
{
	setProperty("class", "PartSwitchToolButton");
	setAutoFillBackground(false); /// musi bejt off
	setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Fixed);
	setToolButtonStyle(Qt::ToolButtonTextUnderIcon);
	setAutoRaise(true);
	setCheckable(true);
	setAutoExclusive(false);

	connect(this, &Super::clicked, [this]() {
		emit partClicked(this->m_partIndex);
	});
}

PartSwitch::PartSwitch(StackedCentralWidget *central_widget, QWidget *parent) :
	Super(parent), m_centralWidget(central_widget), m_currentPartIndex(-1)
{
	setObjectName("partSwitch");
	setWindowTitle(tr("Part switch"));
}


qf::gui::framework::PartSwitch::~PartSwitch()
= default;

void PartSwitch::addPartWidget(PartWidget *widget)
{
	qfLogFuncFrame() << widget << widget->featureId() << widget->title();
	auto *bt = new PartSwitchToolButton();
	bt->setCheckable(true);
	connect(bt, &PartSwitchToolButton::partClicked, this, [this](int ix) {
		setCurrentPartIndex(ix);
	});
	bt->setText(widget->title());
	bt->setPartIndex(buttonCount());
	addWidget(bt);
	{
		QIcon ico = widget->createIcon();
		//bt->setIconSize(QSize{128, 128});
		bt->setIcon(ico);
	}
}

void PartSwitch::setPartVisible(int part_index, bool visible)
{
	for(auto *a : actions()) {
		if(auto *bt = qobject_cast<PartSwitchToolButton*>(widgetForAction(a))) {
			if(bt->partIndex() == part_index) {
				a->setVisible(visible);
				break;
			}
		}
	}
}

void PartSwitch::setCurrentPartIndex(int ix, bool is_active)
{
	qfLogFuncFrame() << m_currentPartIndex << "->" << ix;
	if(!is_active)
		return;
	if(m_currentPartIndex == ix)
		return;
	PartSwitchToolButton *bt1 = buttonAt(m_currentPartIndex);
	PartSwitchToolButton *bt2 = buttonAt(ix);
	bool ok1 = m_centralWidget->setActivePart(m_currentPartIndex, false);
	bool ok2 = false;
	if(ok1) {
		int old_ix = m_currentPartIndex;
		m_currentPartIndex = ix;
		ok2 = m_centralWidget->setActivePart(ix, true);
		if(!ok2) {
			m_currentPartIndex = old_ix;
		}
	}
	if(bt1 && ok2)
		bt1->setChecked(false);
	if(bt2)
		bt2->setChecked(ok2);
}

int PartSwitch::buttonCount()
{
	return findChildren<PartSwitchToolButton*>(QString(), Qt::FindDirectChildrenOnly).count();
}

PartSwitchToolButton *PartSwitch::buttonAt(int part_index)
{
	QList<PartSwitchToolButton*> lst = findChildren<PartSwitchToolButton*>(QString(), Qt::FindDirectChildrenOnly);
	for(auto *bt : lst) {
		if(bt->partIndex() == part_index) {
			return bt;
		}
	}
	return nullptr;
}



