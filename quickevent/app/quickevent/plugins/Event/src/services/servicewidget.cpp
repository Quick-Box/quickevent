#include "servicewidget.h"
#include "ui_servicewidget.h"

#include <qf/gui/style.h>

#include <QDesktopServices>
#include <QUrl>

namespace Event::services {

ServiceWidget::ServiceWidget(QWidget *parent) :
	QWidget(parent),
	ui(new Ui::ServiceWidget)
{
	ui->setupUi(this);

	ui->btPlay->setIcon(qf::gui::Style::icon("media-play"));
	ui->btSettings->setIcon(qf::gui::Style::icon("settings"));
	ui->btHelp->setIcon(qf::gui::Style::icon("help"));
	ui->btHelp->setIconSize(QSize(16, 16));
	ui->btHelp->hide();
	ui->btVisible->setCheckable(true);
	ui->btVisible->setChecked(true);
	ui->btVisible->setIcon(qf::gui::Style::icon("eye"));
	ui->btVisible->setToolTip(tr("Show / hide service in the list"));
	ui->btVisible->hide();

	connect(ui->btPlay, &QAbstractButton::clicked, this, &ServiceWidget::onBtPlayClicked);
	connect(ui->btSettings, &QAbstractButton::clicked, this, &ServiceWidget::showDetail);
	connect(ui->btHelp, &QAbstractButton::clicked, this, [] {
		QDesktopServices::openUrl(QUrl("https://github.com/Quick-Box/quickevent/wiki/Services"));
	});
	connect(ui->btVisible, &QAbstractButton::toggled, this, [this](bool on) {
		setServiceVisible(on);
		emit serviceVisibleToggled(on);
	});
}

ServiceWidget::~ServiceWidget()
{
	delete ui;
}

void ServiceWidget::setStatus(Service::Status st)
{
	m_isRunning = (st == Service::Status::Running);
	static QIcon ico_play = qf::gui::Style::icon("media-play");
	static QIcon ico_stop = qf::gui::Style::icon("media-stop");
	ui->btPlay->setIcon(m_isRunning? ico_stop: ico_play);
	switch (st) {
	case Service::Status::Running:
		ui->lblStatus->setPixmap(QPixmap(":/qf/gui/images/light-green"));
		break;
	case Service::Status::Stopped:
		ui->lblStatus->setPixmap(QPixmap(":/qf/gui/images/light-red"));
		break;
	//case Service::Status::Failed:
	//	ui->lblStatus->setPixmap(QPixmap(":/qf/gui/images/light-red"));
	//	break;
	case Service::Status::Unknown:
		ui->lblStatus->setPixmap(QPixmap(":/qf/gui/images/light-blind"));
		break;
	}
}

void ServiceWidget::setService(Service *service)
{
	m_serviceId = service->serviceId();
	ui->lblServiceName->setText(service->serviceDisplayName());
	setStatus(service->status());
	connect(service, &Service::statusChanged, this, &ServiceWidget::setStatus);
	setMessage(service->statusMessage());
	connect(service, &Service::statusMessageChanged, this, &ServiceWidget::setMessage);
	connect(this, &ServiceWidget::setRunningRequest, service, &Service::setRunning);
	setDescription(service->serviceDescription());
}

QString ServiceWidget::serviceId() const
{
	return m_serviceId;
}

void ServiceWidget::setMessage(const QString &m)
{
	ui->lblServiceMessage->setText(m);
}

void ServiceWidget::setDescription(const QString &d)
{
	ui->btHelp->setVisible(!d.isEmpty());
	// rich text, so that long lines are wrapped
	ui->btHelp->setToolTip("<p>" + d.toHtmlEscaped().replace('\n', "<br>") + "</p>");
}

void ServiceWidget::setServiceVisible(bool on)
{
	QSignalBlocker b(ui->btVisible);
	ui->btVisible->setChecked(on);
	ui->btVisible->setIcon(qf::gui::Style::icon(on? "eye": "eye-off"));
	// grey out hidden services in edit mode
	const bool dim = !on && m_visibilityEditMode;
	for (QWidget *w : {ui->lblStatus, ui->lblServiceName, ui->lblServiceMessage}) {
		w->setEnabled(!dim);
	}
}

void ServiceWidget::setVisibilityEditMode(bool on)
{
	m_visibilityEditMode = on;
	ui->btVisible->setVisible(on);
	setServiceVisible(ui->btVisible->isChecked());
}

void ServiceWidget::onBtPlayClicked()
{
	emit setRunningRequest(!m_isRunning);
}

void ServiceWidget::showDetail()
{
	Service *svc = Service::serviceByName(serviceId());
	if(svc) {
		svc->showDetail(this);
	}
}

}
