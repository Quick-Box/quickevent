//#include "ogsupport/ogsqltablemodel.h"
//#include "ogsupport/ogtimems.h"
//#include "ogsupport/ogtimeedit.h"
#include "coreplugin.h"
//#include "widgets/appstatusbar.h"

#include <qf/core/log.h>

#include <QQmlExtensionPlugin>
#include <qqml.h>

class QmlPlugin : public QQmlExtensionPlugin
{
	Q_OBJECT
	Q_PLUGIN_METADATA(IID "org.qt-project.Qt.QQmlExtensionInterface")
public:
	void registerTypes(const char *uri)
	{
		qfLogFuncFrame() << uri;
		Q_ASSERT(uri == QLatin1String("Core"));

		//qmlRegisterSingletonType<qf::core::qml::QmlLogSingleton>(uri, 1, 0, "Log_helper", &qf::core::qml::QmlLogSingleton::singletontype_provider);
		//qmlRegisterType<OGSqlTableModel>(uri, 1, 0, "OGSqlTableModel");

		qmlRegisterType<CorePlugin>(uri, 1, 0, "CorePlugin");
		//qmlRegisterType<AppStatusBar>(uri, 1, 0, "AppStatusBar");
	}
};

#include "plugin.moc"
