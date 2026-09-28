#ifndef QF_GUI_FRAMEWORK_REPORTFILECACHE_H
#define QF_GUI_FRAMEWORK_REPORTFILECACHE_H

#include "../guiglobal.h"

#include <QObject>

namespace qf::gui::framework {

class QFGUI_DECL_EXPORT ReportFileCache : public QObject
{
	Q_OBJECT
public:
	QString localReportFile(const QString &relative_path) const;
	QString localReportsDir() const;
	QString reportCacheDir() const;
	void applyDatabaseOverrides() const;
	void clearLocalChanges();
	bool saveRemoteFileContent(const QString &relative_path, const QByteArray &data, bool update_local_copy = true) const;
	QByteArray loadReportFile(const QString &relative_path) const;

	static QString dataHash(const QByteArray &data);
	static QString fileHash(const QString &file_path);
private:
	void initIfNotExists() const;
private:
	friend class Plugin;
	ReportFileCache();
};

}

#endif // QF_GUI_FRAMEWORK_REPORTFILECACHE_H
