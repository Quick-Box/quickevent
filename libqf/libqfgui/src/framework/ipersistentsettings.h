#ifndef QF_GUI_FRAMEWORK_IPERSISTENTSETTINGS_H
#define QF_GUI_FRAMEWORK_IPERSISTENTSETTINGS_H

#include "../guiglobal.h"

#include <QString>

class QObject;

namespace qf::gui::framework {

class QFGUI_DECL_EXPORT IPersistentSettings
{
public:
	IPersistentSettings(QObject *controlled_object);
	virtual ~IPersistentSettings();
public:
	virtual QString persistentSettingsId();
	virtual bool setPersistentSettingsId(const QString &id);

	virtual QString persistentSettingsPath();

	virtual void loadPersistentSettingsRecursively();
	//virtual void savePersistentSettingsRecursively();

	static const QString& defaultPersistentSettingsPathPrefix();
	QString persistentSettingsPathPrefix();
	void setPersistentSettingsPathPrefix(const QString &prefix);
protected:
	virtual QString effectivePersistentSettingsPathPrefix();
	virtual QString rawPersistentSettingsPath();
private:
	QObject *m_controlledObject;
	QString m_persistentSettingsId;
	QString m_persistentSettingsPathPrefix;
	QString m_persistentSettingsPath;
};

}

#endif
