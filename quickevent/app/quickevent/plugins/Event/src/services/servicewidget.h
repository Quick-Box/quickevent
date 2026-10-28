#ifndef SERVICES_SERVICEWIDGET_H
#define SERVICES_SERVICEWIDGET_H

#include "service.h"

#include <QWidget>

namespace Event {
namespace services {

namespace Ui {
class ServiceWidget;
}

class ServiceWidget : public QWidget
{
	Q_OBJECT

public:
	explicit ServiceWidget(QWidget *parent = 0);
	~ServiceWidget();

	void setStatus(Service::Status st);
	void setService(Service *service);
	QString serviceId() const;
	bool isRunning() const {return m_isRunning;}
	void setMessage(const QString &m);
	void setDescription(const QString &d);
	void setServiceVisible(bool on);
	void setVisibilityEditMode(bool on);

	Q_SIGNAL void setRunningRequest(bool play);
	Q_SIGNAL void serviceVisibleToggled(bool on);
private:
	void onBtPlayClicked();
	void showDetail();
private:
	Ui::ServiceWidget *ui;
	bool m_isRunning = false;
	bool m_visibilityEditMode = false;
	QString m_serviceId;
};


}}

#endif // SERVICES_SERVICEWIDGET_H
