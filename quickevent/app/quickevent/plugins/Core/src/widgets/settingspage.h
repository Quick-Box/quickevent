#ifndef CORE_SETTINGSPAGEWIDGET_H
#define CORE_SETTINGSPAGEWIDGET_H

#include <QWidget>

namespace Core {

class SettingsPage : public QWidget
{
	Q_OBJECT

public:
	explicit SettingsPage(QWidget *parent = nullptr);

	QString caption() const { return m_caption; }
	bool requiresOpenEvent() const { return m_requiresOpenEvent; }

	virtual void load() = 0;
	virtual void save() = 0;
protected:
	QString m_caption;
	bool m_requiresOpenEvent = false;
	//QString m_settingsDir;
};

} // namespace Core

#endif // CORE_SETTINGSPAGEWIDGET_H
