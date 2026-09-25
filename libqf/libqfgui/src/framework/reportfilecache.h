#ifndef QF_GUI_FRAMEWORK_REPORTFILECACHE_H
#define QF_GUI_FRAMEWORK_REPORTFILECACHE_H

#include "../guiglobal.h"

#include <QObject>

namespace qf::gui::framework {

class QFGUI_DECL_EXPORT ReportFileCache : public QObject
{
	Q_OBJECT
public:
	QString effectiveReportsDir() const;
	QString reportCacheDir() const;
	void applyDatabaseOverrides() const;
	void clearLocalChanges();
private:
	void initIfNotExists();
	QString defaultReportsDir() const;
private:
	friend class Plugin;
	ReportFileCache();
};

}

#endif // QF_GUI_FRAMEWORK_REPORTFILECACHE_H
