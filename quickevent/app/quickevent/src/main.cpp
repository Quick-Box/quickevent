#include "mainwindow.h"
#include "application.h"
#include "appversion.h"
#include "appclioptions.h"

#include <plugins/Core/src/coreplugin.h>

#include <quickevent/core/si/siid.h>
#include <quickevent/core/og/timems.h>

#include <qf/core/sql/qxsql.h>
#include <qf/core/log.h>
#include <qf/core/logentrymap.h>
#include <qf/core/utils/settings.h>
#include <qf/gui/model/logtablemodel.h>
#include <qf/gui/framework/plugin.h>
#include <qf/gui/framework/reportfilecache.h>

#include <QLocale>
#include <QSslSocket>
#include <QTranslator>
#include <QLibraryInfo>

namespace {
NecroLog::MessageHandler old_message_handler;
bool send_log_entry_recursion_lock = false;

void send_log_entry_handler(NecroLog::Level level, const NecroLog::LogContext &context, const std::string &msg)
{
	if(!send_log_entry_recursion_lock) {
		send_log_entry_recursion_lock = true;
		Application *app = Application::instance();
		if(app) {
			qf::core::LogEntryMap le;
			le.setTimeStamp(QDateTime::currentDateTime());
			le.setLevel(level);
			le.setCategory(context.topic());
			le.setFile(context.file());
			le.setLine(context.line());
			le.setMessage(QString::fromStdString(msg));
			app->emitNewLogEntry(le);
		}
		send_log_entry_recursion_lock = false;
	}
	old_message_handler(level, context, msg);
}
}

int main(int argc, char *argv[])
{
	//dump_resources();
	QCoreApplication::setOrganizationName("quickbox");
	QCoreApplication::setOrganizationDomain("quickbox.org");
	QCoreApplication::setApplicationName("quickevent");
	QCoreApplication::setApplicationVersion(APP_VERSION);

	std::vector<std::string> shv_args = NecroLog::setCLIOptions(argc, argv);
	QStringList args;
	for(const auto &s : shv_args) {
		args << QString::fromStdString(s);
	}

	QSettings::setDefaultFormat(QSettings::IniFormat);

#if QT_VERSION_MAJOR < 6
	QApplication::setAttribute(Qt::AA_DisableWindowContextHelpButton);
#endif

#ifdef Q_OS_MACOS
	// Disable native macOS menu bar - keeps all menus in the window
	QApplication::setAttribute(Qt::AA_DontUseNativeMenuBar);
	// Use Fusion style on macOS for consistent cross-platform appearance
	QApplication::setStyle("Fusion");
#endif

	qfInfo() << "========================================================";
	qfInfo() << QDateTime::currentDateTime().toString(Qt::ISODate) << "starting" << QCoreApplication::applicationName() << "ver:" << QCoreApplication::applicationVersion();
	qfInfo() << "Log tresholds:" << NecroLog::thresholdsLogInfo();
	qfInfo() << "Open SSL:" << QSslSocket::sslLibraryBuildVersionString();

	qRegisterMetaType<qf::core::LogEntryMap>();
	quickevent::core::og::TimeMs::registerQVariantFunctions();
	quickevent::core::si::SiId::registerQVariantFunctions();

	AppCliOptions cli_opts;
	cli_opts.parse(args);
	if(cli_opts.isParseError()) {
		foreach(QString err, cli_opts.parseErrors())
			qfError() << err;
		return EXIT_FAILURE;
	}
	if(cli_opts.isAppBreak()) {
		if(cli_opts.isHelp())
			cli_opts.printHelp();
		return EXIT_SUCCESS;
	}
	foreach(QString s, cli_opts.unusedArguments()) {
		qfWarning() << "Undefined argument:" << s;
	}

	// Uncaught exception is intentional here
	if(!cli_opts.loadConfigFile()) {
		return EXIT_FAILURE;
	}

	Application app(argc, argv, &cli_opts);

	qfInfo() << "Abort on exception:" << qf::core::Exception::isAbortOnException();
	qfInfo() << "Application file:" << QCoreApplication::applicationFilePath();
	qfInfo() << "Application dir:" << QCoreApplication::applicationDirPath();
	qfInfo() << "========================================================";

	QString lc_name;
	{
		if(cli_opts.locale_isset()) {
			lc_name = cli_opts.locale();
		}
		else {
			qf::core::utils::Settings settings;
			lc_name = settings.value(Core::CorePlugin::SETTINGS_PREFIX_APPLICATION_LOCALE_LANGUAGE()).toString();
		}
		if(lc_name.isEmpty() || lc_name == QLatin1String("system"))
			lc_name = QLocale::system().name();

		auto lc_dir = QCoreApplication::applicationDirPath() + "/translations";
		qfInfo() << "Loading translations for:" << lc_name << "search dir:" << lc_dir;

		for(const auto &file_name : {
				QStringLiteral("libqfcore"),
				QStringLiteral("libqfgui"),
				QStringLiteral("libquickeventcore"),
				QStringLiteral("libquickeventgui"),
				QStringLiteral("libsiut"),
				QStringLiteral("quickevent"),
		}) {
			auto *translator = new QTranslator(&app);
			bool ok = translator->load(QLocale(lc_name), file_name, QString("-"), lc_dir);
			if (ok) {
				ok = QCoreApplication::installTranslator(translator);
			}
			qfInfo() << "Installing translator file:" << file_name << " ... " << (ok? "OK": "ERROR");
		}

		{
			auto *translator = new QTranslator(&app);
			const auto file_name = QStringLiteral("qt");
			// Load Qt's own catalog (standard QMessageBox buttons etc.). Try the
			// translations dir next to the executable first (installation layout),
			// then fall back to Qt's installed translations (typical for dev builds).
			bool ok = translator->load(QLocale(lc_name), file_name, QString("_"), lc_dir);
			if (!ok) {
				ok = translator->load(QLocale(lc_name), file_name, QString("_"),
									  QLibraryInfo::path(QLibraryInfo::TranslationsPath));
			}
			if (ok) {
				ok = QCoreApplication::installTranslator(translator);
			}
			qfInfo() << "Installing translator file:" << file_name << "... " << (ok ? "OK" : "ERROR");
		}
	}

	MainWindow main_window;

	// does nothing before MainWindow is created
	old_message_handler = NecroLog::setMessageHandler(send_log_entry_handler);

	main_window.setUiLanguageName(lc_name);
	main_window.loadPlugins();
	main_window.show();
	emit main_window.applicationLaunched();

#ifdef QT_DEBUG
	QObject::connect(app.qxSql(), &qf::core::sql::QxSql::recChng, &app, [](const qf::core::sql::QxRecChng &recchng) {
		auto dump_map = [](const QVariantMap &m) {
			QStringList rows;
			for (const auto &[k, v] : m.asKeyValueRange()) {
				rows << QStringLiteral("%1 -> %2").arg(k).arg(v.toString());
			}
			return rows.join('\n');
		};
		qfInfo() << "REC-CHNG table:" << recchng.table
				 << "op:" << qf::core::sql::QxRecChng::recopToString(recchng.op)
				 << "id:" << recchng.id
				 << "record:" << dump_map(recchng.record);
	});
#endif

	int ret = Application::exec();

	return ret;
}
