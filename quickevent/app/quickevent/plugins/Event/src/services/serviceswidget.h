#pragma once

#include <QWidget>

class QToolButton;

namespace Event {
namespace services {

class ServiceWidget;

class ServicesWidget : public QWidget
{
	Q_OBJECT
public:
	explicit ServicesWidget(QWidget *parent = nullptr);

	void reload();
private:
	void updateVisibility();
	void setServiceHidden(const QString &service_id, bool hidden);
private:
	QWidget *m_centralWidget = nullptr;
	QToolButton *m_btEditVisibility = nullptr;
	QList<ServiceWidget*> m_serviceWidgets;
	// hidden list instead of visible one, so that newly added services are shown by default
	QStringList m_hiddenServices;
};

}}
