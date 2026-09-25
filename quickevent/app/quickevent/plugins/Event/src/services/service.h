#pragma once

#include <qf/core/utils.h>

#include <QObject>

namespace qf { namespace gui { namespace framework { class DialogWidget; }}}
namespace Event { class EventPlugin; }

namespace Event {
namespace services {

class ServiceSettings : public QVariantMap
{
	using Super = QVariantMap;

	QF_VARIANTMAP_FIELD(bool, is, set, AutoStart)

public:
	ServiceSettings(const QVariantMap &o = QVariantMap()) : Super(o) {}
};

class Service : public QObject
{
	Q_OBJECT

	QF_PROPERTY_IMPL(QString, s, S, tatusMessage)
public:
	enum class Status { Unknown, Stopped, Running};
public:
	explicit Service(const QString &name, QObject *parent = nullptr);
	~Service() override;

	QString serviceId() const {return objectName();}
	virtual QString serviceDisplayName() const {return serviceId();}

	virtual void run();
	virtual void stop();
	void restart()
	{
		stop();
		loadSettings();
		run();
	}

	void loadSettingsForEvent(const QString &event_name);
	ServiceSettings settings() const {return ServiceSettings(m_settings);}
	void saveSettings();
	void setSettings(const QVariantMap &s);
	Q_SIGNAL void settingsChanged();

	void showDetail(QWidget *parent);

	Status status() const {return m_status;}
	Q_SIGNAL void statusChanged(Status new_status);

	virtual void setRunning(bool on);
	bool isRunning() const { return status() == Status::Running; }

	static void addService(Service *service);
	static int serviceCount() {return m_services.count();}
	static Service* serviceAt(int ix);
	static Service* serviceByName(const QString &service_name);
protected:
	virtual void loadSettings();
	QString settingsGroup() const;
	virtual void onEventOpen();
	void setStatus(Status st)
	{
		if(st == status())
			return;
		m_status = st;
		emit statusChanged(st);
	}

	virtual qf::gui::framework::DialogWidget *createDetailWidget();
protected:
	QVariantMap m_settings;
private:
	QString m_eventName;
	Status m_status = Status::Unknown;
	static QList<Service*> m_services;
};

}}
