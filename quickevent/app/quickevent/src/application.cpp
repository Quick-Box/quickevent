#include "application.h"
#include "appclioptions.h"

#include <qf/gui/reports/processor/reportprocessor.h>
#include <qf/gui/style.h>

#include <qf/core/log.h>

#include <QNetworkProxy>
#include <QProcessEnvironment>

#ifdef Q_OS_WIN
#include <windows.h>
#endif

Application::Application(int &argc, char **argv, AppCliOptions *cli_opts)
	: Super(argc, argv)
	, m_cliOptions(cli_opts)
{
	if(cli_opts->appFontScale() != 1.) {
		QFont app_font = font();
		app_font.setPointSizeF(app_font.pointSizeF() * cli_opts->appFontScale());
		setFont(app_font);
	}

	auto *style = qf::gui::Style::instance();
	style->addIconSearchPath(":/qf/gui/images/lucide");
	style->addIconSearchPath(":/qf/gui/images/flat");
	style->addIconSearchPath(":/qf/gui/images");

	if (qf::gui::isDarkTheme()) {
		loadStyleSheet(":/quickevent/style/dark.css");
	}
	else {
		loadStyleSheet(":/quickevent/style/default.css");
	}

	QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
	QString http_proxy = env.value(QStringLiteral("http_proxy"));
	if(!http_proxy.isEmpty()) {
		if(http_proxy.startsWith(QLatin1String("http://")))
			http_proxy = http_proxy.mid(7);
		QString host = http_proxy.section(':', 0, 0);
		int port = http_proxy.section(':', 1).toInt();
		if(!host.isEmpty() && port > 0) {
			qfInfo() << QString("Setting app http proxy to: %1:%2").arg(host).arg(port);
			QNetworkProxy proxy;
			proxy.setType(QNetworkProxy::HttpProxy);
			proxy.setHostName(host);
			proxy.setPort(port);
			//proxy.setUser("username");
			//proxy.setPassword("password");
			QNetworkProxy::setApplicationProxy(proxy);
		}
	}
#ifdef Q_OS_UNIX
	//QString plugin_path = QCoreApplication::applicationDirPath() + "/../lib/qml/" + QCoreApplication::applicationName().toLower();
#else
	QString plugin_path = QCoreApplication::applicationDirPath() + "/qml/" + QCoreApplication::applicationName().toLower();
#ifdef Q_OS_WIN
	qfInfo() << "Adding DLL search path:" << plugin_path;
	SetDllDirectory(reinterpret_cast<LPCWSTR>(plugin_path.utf16()));
#endif
#endif
	//qf::gui::reports::ReportProcessor::qmlEngineImportPaths().append(plugin_path);
}

Application *Application::instance(bool must_exist)
{
	auto *ret = qobject_cast<Application*>(Super::instance());
	if(!ret && must_exist) {
		qfFatal("Application instance MUST exist.");
	}
	return ret;
}

