#ifndef QF_GUI_FRAMEWORK_REPORTFILECACHE_H
#define QF_GUI_FRAMEWORK_REPORTFILECACHE_H

#include "../guiglobal.h"

#include <QObject>

namespace qf {
namespace gui {
namespace framework {

class QFGUI_DECL_EXPORT ReportFileCache : public QObject
{
	Q_OBJECT
public:
	QString effectiveReportsDir() const;
	QString reportCacheDir() const;
	void applyDatabaseOverrides() const;
private:
	void initIfNotExists() const;
	QString defaultReportsDir() const;
private:
	friend class Plugin;
	ReportFileCache();
};

}}}

#endif // QF_GUI_FRAMEWORK_REPORTFILECACHE_H
