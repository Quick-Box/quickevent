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

	void setReportsDir(const QString &dir) { m_reportsDir = dir; }
	QString effectiveReportsDir() const;
	QString defaultReportsDir() const;
	QString reportCacheDir() const;
	void initialize() const;
	void applyDatabaseOverrides() const;

private:
	friend class Plugin;
	ReportFileCache();
	QString m_reportsDir;
};

}}}

#endif // QF_GUI_FRAMEWORK_REPORTFILECACHE_H
