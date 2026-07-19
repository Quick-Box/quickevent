#include "service.h"

#include "../eventplugin.h"

#include <qf/gui/framework/dialogwidget.h>
#include <qf/gui/framework/mainwindow.h>
#include <qf/gui/dialogs/dialog.h>

#include <qf/core/assert.h>

#include <QSettings>

namespace qff = qf::gui::framework;
namespace qfd = qf::gui::dialogs;
using qf::gui::framework::getPlugin;
using Event::EventPlugin;


namespace Event::services {

QList<Service*> Service::m_services; // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)

Service::Service(const QString &name, QObject *parent)
	: QObject(parent)
{
	//qfDebug() << name;
	setObjectName(name);
	setStatus(Status::Stopped);
	connect(getPlugin<EventPlugin>(), &Event::EventPlugin::eventOpenChanged, this, &Service::onEventOpen, Qt::QueuedConnection);
}

Service::~Service()
{
	bool is_running = status() == Status::Running;
	ServiceSettings ss = settings();
	ss.setAutoStart(is_running);
	setSettings(ss);
	saveSettings();
}

QString Service::settingsGroup() const
{
	if (m_eventName.isEmpty()) {
		return QStringLiteral("services/%1").arg(serviceId());
	}
	return QStringLiteral("services/%1/%2").arg(m_eventName, serviceId());
}

void Service::onEventOpen()
{
	auto *event_plugin = getPlugin<EventPlugin>();
	if(event_plugin->isEventOpen()) {
		loadSettingsForEvent(event_plugin->eventDbName());
		ServiceSettings ss = settings();
		if(ss.isAutoStart()) {
			run();
		}
	}
}

void Service::loadSettings()
{
	qfDebug() << "loading settings for" << settingsGroup();
	m_settings.clear();
	QSettings ss;
	ss.beginGroup(settingsGroup());
	for(const QString &key : ss.childKeys()) {
		m_settings[key] = ss.value(key);
		qfDebug() << "\t" << key << "->" << m_settings.value(key);
	}
}

void Service::saveSettings()
{
	QSettings ss;
	ss.beginGroup(settingsGroup());
	for(const auto &[key, _] : m_settings.asKeyValueRange()) {
		ss.setValue(key, m_settings[key]);
	}
}

void Service::setSettings(const QVariantMap &s)
{
	//qfInfo() << __FUNCTION__ << settingsGroup() << s;
	if(!(m_settings == s)) {
		m_settings = s;
		emit settingsChanged();
	}
}

void Service::run()
{
	setStatus(Status::Running);
}

void Service::stop()
{
	setStatus(Status::Stopped);
}

void Service::loadSettingsForEvent(const QString &event_name)
{
	m_eventName = event_name;
	loadSettings();
}

void Service::setRunning(bool on)
{
	if(on && status() == Status::Stopped) {
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
	dlg.setWindowTitle(serviceId());
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
		if(svc->serviceId() == service_name) {
			return svc;
		}
	}
	return nullptr;
}

qf::gui::framework::DialogWidget *Service::createDetailWidget()
{
	return nullptr;
}

}
