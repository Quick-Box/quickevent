#include "typstexecutable.h"

#include <qf/gui/framework/plugin.h>
#include <qf/gui/framework/reportfilecache.h>

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QStandardPaths>

namespace Typst {

QString executablePath()
{
	QString name = QStringLiteral("typst");
#ifdef Q_OS_WIN
	name += QStringLiteral(".exe");
#endif
	const QString bundled = QCoreApplication::applicationDirPath() + QLatin1Char('/') + name;
	if (QFile::exists(bundled))
		return bundled;
	return QStandardPaths::findExecutable(QStringLiteral("typst"));
}

QString fontsDir()
{
	// fonts are report files (resources -> local cache, optionally overridden in SQL)
	const QString reports_dir = qf::gui::framework::Plugin::reportFileCache()->localReportsDir();
	if (reports_dir.isEmpty()) {
		return {};
	}
	const QString dir = reports_dir + QStringLiteral("/shared/reports/fonts");
	return QDir(dir).exists() ? dir : QString();
}

} // namespace Typst
