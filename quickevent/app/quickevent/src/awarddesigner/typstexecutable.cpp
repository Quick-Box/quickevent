#include "typstexecutable.h"

#include <QCoreApplication>
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

} // namespace Typst
