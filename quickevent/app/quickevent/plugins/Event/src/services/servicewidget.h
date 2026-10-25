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
	void setMessage(const QString &m);

	Q_SIGNAL void setRunningRequest(bool play);
private:
	void onBtPlayClicked();
	void showDetail();
private:
	Ui::ServiceWidget *ui;
	bool m_isRunning = false;
	QString m_serviceId;
};


}}

#endif // SERVICES_SERVICEWIDGET_H
