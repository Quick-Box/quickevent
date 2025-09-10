#include "application.h"
#include "appclioptions.h"
#include "mainwindow.h"

#include <qf/core/log.h>

#include <QLocale>
#include <QSettings>
#include <QTranslator>

int main(int argc, char *argv[])
{
	QSettings::setDefaultFormat(QSettings::IniFormat);
	QCoreApplication::setOrganizationDomain("quickbox.org");
	QCoreApplication::setOrganizationName("quickbox");
	QCoreApplication::setApplicationName("quickshow");

	std::vector<std::string> shv_args = NecroLog::setCLIOptions(argc, argv);
	QStringList args;
	for(const auto &s : shv_args)
		args << QString::fromStdString(s);

	AppCliOptions cli_opts;

	cli_opts.parse(args);
	if(cli_opts.isParseError()) {
		foreach(QString err, cli_opts.parseErrors())
			qfError() << err;
		return 1;
	}
	if(cli_opts.isAppBreak()) {
		if(cli_opts.isHelp())
			cli_opts.printHelp();
		return 0;
	}
	foreach(QString s, cli_opts.unusedArguments()) {
		qfInfo() << "Undefined argument:" << s;
	}

	// Uncaught exception is intentional here
	if(!cli_opts.loadConfigFile())
		return 1;

	Application app(argc, argv, &cli_opts);

	QString lc_name;
	{
		if(cli_opts.locale_isset())
			lc_name = cli_opts.locale();
		if(lc_name.isEmpty() || lc_name == QLatin1String("system"))
			lc_name = QLocale::system().name();

		qfInfo() << "Loading translations for:" << lc_name;

		for(const auto &file_name : {
			QStringLiteral("libqfcore"),
			QStringLiteral("libqfgui"),
			QStringLiteral("quickshow"),
		}) {
			QTranslator *translator = new QTranslator(&app);
			bool ok = translator->load(QLocale(lc_name), file_name, QString("-"), QString(":/i18n"));
			if (ok) {
				ok = QCoreApplication::installTranslator(translator);
			}
			qfInfo() << "Installing translator file:" << file_name << " ... " << (ok? "OK": "ERROR");
		}
		{
			QTranslator *translator = new QTranslator(&app);
			const auto file_name = QStringLiteral("qt");
			bool ok = translator->load(QLocale(lc_name), file_name, QString("_"), QString("translations"));
			if (ok) {
				ok = QCoreApplication::installTranslator(translator);
			}
			qfInfo() << "Installing translator file:" << file_name << "... " << (ok ? "OK" : "ERROR");
		}
	}

	MainWindow w;
	w.show();

	return app.exec();
}
