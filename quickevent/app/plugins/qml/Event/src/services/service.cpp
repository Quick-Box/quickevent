#include "service.h"

#include "../Event/eventplugin.h"

#include <qf/qmlwidgets/framework/dialogwidget.h>
#include <qf/qmlwidgets/framework/mainwindow.h>
#include <qf/qmlwidgets/dialogs/dialog.h>

#include <qf/core/assert.h>

#include <QSettings>

namespace qff = qf::qmlwidgets::framework;
namespace qfd = qf::qmlwidgets::dialogs;

namespace services {

static const char *KEY_IS_RUNNING = "isRunning";

QList<Service*> Service::m_services;

Service::Service(const QString &name, QObject *parent)
	: QObject(parent)
{
	//qfDebug() << name;
	setObjectName(name);
	setStatus(Status::Stopped);
	connect(eventPlugin(), &Event::EventPlugin::eventOpened, this, &Service::onEventOpen, Qt::QueuedConnection);
}

Service::~Service()
{
	bool is_running = status() == Status::Running;
	QSettings settings;
	settings.beginGroup(settingsGroup());
	settings.setValue(KEY_IS_RUNNING, is_running);
}

Event::EventPlugin *Service::eventPlugin()
{
	qf::qmlwidgets::framework::MainWindow *fwk = qf::qmlwidgets::framework::MainWindow::frameWork();
	auto *plugin = qobject_cast<Event::EventPlugin *>(fwk->plugin("Event"));
	QF_ASSERT(plugin != nullptr, "Bad plugin", return nullptr);
	return plugin;
}

QString Service::settingsGroup() const
{
	QString s = QStringLiteral("services/") + name();
	return s;
}

void Service::onEventOpen()
{
	loadSettings();
	QSettings settings;
	settings.beginGroup(settingsGroup());
	bool is_running = settings.value(KEY_IS_RUNNING).toBool();
	qfDebug() << this << settingsGroup() << KEY_IS_RUNNING << is_running;
	if(is_running) {
		run();
	}
}

void Service::loadSettings()
{
}

void Service::run()
{
	//setStatus(Status::Starting);
	setStatus(Status::Running);
}

void Service::stop()
{
	setStatus(Status::Stopped);
}

void Service::setRunning(bool on)
{
	if(on && status() == Status::Stopped) {
		loadSettings();
		run();
	}
	else if(!on && status() == Status::Running) {
		stop();
	}
}

void Service::showDetail(QWidget *parent)
{
	qff::DialogWidget *cw = createDetailWidget();
	if(!cw)
		return;
	qfd::Dialog dlg(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, parent);
	dlg.setCentralWidget(cw);
	dlg.setWindowTitle(name());
	dlg.exec();
}

void Service::addService(Service *service)
{
	if(service)
		m_services << service;
}

Service *Service::serviceAt(int ix)
{
	return m_services.at(ix);
}

Service *Service::serviceByName(const QString &service_name)
{
	for (int i = 0; i < serviceCount(); ++i) {
		Service *svc = serviceAt(i);
		if(svc->name() == service_name) {
			return svc;
		}
	}
	return nullptr;
}

qf::qmlwidgets::framework::DialogWidget *Service::createDetailWidget()
{
	return nullptr;
}

} // namespace services
