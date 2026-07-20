#include "serviceswidget.h"
#include "service.h"
#include "servicewidget.h"

#include <QHBoxLayout>
#include <QScrollArea>


namespace Event::services {

ServicesWidget::ServicesWidget(QWidget *parent)
	: QWidget(parent)
{
	new QHBoxLayout(this);
}

void ServicesWidget::reload()
{
	delete m_centralWidget;
	auto *sa = new QScrollArea();
	sa->setFrameShape(QFrame::NoFrame);
	m_centralWidget = sa;
	QLayout *ly = layout();
	ly->addWidget(m_centralWidget);

	auto *ly2 = new QVBoxLayout(m_centralWidget);
#if QT_VERSION_MAJOR >= 6
	ly2->setContentsMargins({});
#else
	ly2->setMargin(0);
#endif
	ly->setSpacing(0);

	for (int i = 0; i < Service::serviceCount(); ++i) {
		Service *svc = Service::serviceAt(i);

		auto *sw = new ServiceWidget();
		sw->setService(svc);

		ly2->addWidget(sw);
	}
	ly2->addStretch();
}

}
