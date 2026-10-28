#include "serviceswidget.h"
#include "service.h"
#include "servicewidget.h"

#include <qf/gui/style.h>

#include <QHBoxLayout>
#include <QScrollArea>
#include <QSettings>
#include <QToolButton>


namespace Event::services {

static constexpr auto HIDDEN_SERVICES_KEY = "services/hiddenServices";

ServicesWidget::ServicesWidget(QWidget *parent)
	: QWidget(parent)
{
	m_hiddenServices = QSettings().value(HIDDEN_SERVICES_KEY).toStringList();

	auto *ly = new QVBoxLayout(this);
	auto *ly_header = new QHBoxLayout();
	ly_header->addStretch();
	m_btEditVisibility = new QToolButton();
	m_btEditVisibility->setCheckable(true);
	m_btEditVisibility->setAutoRaise(true);
	m_btEditVisibility->setIcon(qf::gui::Style::icon("eye"));
	m_btEditVisibility->setToolTip(tr("Choose visible services"));
	connect(m_btEditVisibility, &QToolButton::toggled, this, &ServicesWidget::updateVisibility);
	ly_header->addWidget(m_btEditVisibility);
	ly->addLayout(ly_header);
}

void ServicesWidget::reload()
{
	delete m_centralWidget;
	auto *sa = new QScrollArea();
	sa->setFrameShape(QFrame::NoFrame);
	sa->setWidgetResizable(true);
	m_centralWidget = sa;
	QLayout *ly = layout();
	ly->addWidget(m_centralWidget);

	auto *content = new QWidget();
	auto *ly2 = new QVBoxLayout(content);
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
		connect(sw, &ServiceWidget::serviceVisibleToggled, this, [this, id = svc->serviceId()](bool on) {
			setServiceHidden(id, !on);
		});
		// running service stays visible even when hidden
		connect(svc, &Service::statusChanged, sw, [this] {
			updateVisibility();
		});

		ly2->addWidget(sw);
	}
	ly2->addStretch();
	sa->setWidget(content);
	updateVisibility();
}

void ServicesWidget::updateVisibility()
{
	const bool edit_mode = m_btEditVisibility->isChecked();
	for (auto *sw : findChildren<ServiceWidget*>()) {
		const bool visible = !m_hiddenServices.contains(sw->serviceId());
		sw->setVisibilityEditMode(edit_mode);
		sw->setServiceVisible(visible);
		sw->setVisible(edit_mode || visible || sw->isRunning());
	}
}

void ServicesWidget::setServiceHidden(const QString &service_id, bool hidden)
{
	m_hiddenServices.removeAll(service_id);
	if (hidden) {
		m_hiddenServices << service_id;
	}
	QSettings().setValue(HIDDEN_SERVICES_KEY, m_hiddenServices);
}

}
