#include "stackedcentralwidget.h"
#include "partwidget.h"
#include "mainwindow.h"
#include "partswitch.h"

#include <qf/core/log.h>
#include <qf/core/assert.h>

#include <QStackedWidget>
#include <QVBoxLayout>
#include <QMetaMethod>



namespace qf::gui::framework {

StackedCentralWidget::StackedCentralWidget(MainWindow *parent) :
	Super(parent)
{
	qfLogFuncFrame();
	m_centralWidget = new QStackedWidget(this);
	QBoxLayout *ly = new QVBoxLayout(this);
	ly->setContentsMargins(0, 0, 0, 0);
	//qfDebug() << "\t adding:" << m_centralFrame << "to layout:" << ly;
	ly->addWidget(m_centralWidget);
	setLayout(ly);

	m_partSwitch = new PartSwitch(this);
	connect(this, &StackedCentralWidget::partActivated, m_partSwitch, &PartSwitch::setCurrentPartIndex);
	MainWindow::frameWork()->addToolBar(Qt::LeftToolBarArea, m_partSwitch);
}

StackedCentralWidget::~StackedCentralWidget()
= default;

void StackedCentralWidget::addPartWidget(PartWidget *widget)
{
	qfLogFuncFrame() << widget;
	widget->setParent(nullptr);
	m_centralWidget->addWidget(widget);
	m_partSwitch->addPartWidget(widget);
	//widget->updateCaptionFrame();
	QMetaObject::invokeMethod(widget, "updateCaptionFrame", Qt::QueuedConnection);
}

bool StackedCentralWidget::setActivePart(int part_index, bool set_active)
{
	qfLogFuncFrame() << "part index:" << part_index << "set active:" << set_active;
	bool ret = true;
	PartWidget *pw = partWidget(part_index);
	if(pw) {
		qfDebug() << "featureId:" << pw->featureId();
		QVariant ret_val(true);
		int ix = pw->metaObject()->indexOfMethod("canActivate(QVariant)");
		if(ix >= 0) {
			pw->metaObject()->method(ix).invoke(pw, Q_RETURN_ARG(QVariant, ret_val), Q_ARG(QVariant, set_active));
		}
		ret = ret_val.toBool();
		if(ret) {
			pw->setActive(set_active);
			if(set_active)
				m_centralWidget->setCurrentIndex(part_index);
			// emit partActivatedS(pw->featureId(), set_active);
			emit partActivated(part_index, set_active);
		}
	}
	return ret;
}

void StackedCentralWidget::setPartVisible(const QString &feature_id, bool visible)
{
	int ix = featureToIndex(feature_id);
	if(ix >= 0)
		m_partSwitch->setPartVisible(ix, visible);
}

int StackedCentralWidget::featureToIndex(const QString &feature_id)
{
	for (int i = 0; i < m_centralWidget->count(); ++i) {
		auto *pw = partWidget(i);
		if(pw->featureId() == feature_id) {
			return i;
		}
	}
	return -1;
}

PartWidget *StackedCentralWidget::partWidget(int part_index)
{
	auto *pw = qobject_cast<PartWidget*>(m_centralWidget->widget(part_index));
	if(pw == nullptr) {
		//qfWarning() << "invalid part index:" << part_index << "of count:" << m_centralWidget->count();
	}
	return pw;
}

}
