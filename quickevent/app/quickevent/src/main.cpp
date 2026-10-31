#include "mainwindow.h"
#include "application.h"
#include "appversion.h"
#include "appclioptions.h"

#include <plugins/Core/src/coreplugin.h>
#include <plugins/Event/src/eventplugin.h>
#include <plugins/Event/src/dbschema.h>

#include <quickevent/core/si/siid.h>
#include <quickevent/core/og/timems.h>

#include <qf/core/sql/qxsql.h>
#include <qf/core/log.h>
#include <qf/core/logentrymap.h>
#include <qf/core/utils/settings.h>
#include <qf/gui/model/logtablemodel.h>
#include <qf/gui/framework/plugin.h>
#include <qf/gui/framework/reportfilecache.h>

#include <QtQml>
#include <QLocale>
#include <QLibraryInfo>
#include <QClipboard>
#include <QDesktopServices>
#include <QDir>
#include <QMessageBox>
#include <QPushButton>
#include <QScopeGuard>
#include <QStandardPaths>
#include <QSysInfo>

#include <cstdio>
#include <sstream>

#ifdef Q_OS_WIN
#include <qt_windows.h>
#include <ole2.h>
#endif

namespace {
NecroLog::MessageHandler file_log_old_message_handler;
FILE *file_log = nullptr;
QString current_log_path;

void file_log_handler(NecroLog::Level level, const NecroLog::LogContext &context, const std::string &msg)
{
	// stdio FILE is locked internally, no mutex needed
	if (file_log) {
		std::ostringstream os;
		NecroLog::writeWithDefaultFormat(os, false, level, context, msg);
		std::fputs(os.str().c_str(), file_log);
		std::fflush(file_log);
	}
	file_log_old_message_handler(level, context, msg);
}

// Log is written to current.log, which is deleted on clean exit.
// current.log found at startup means, that previous run did not exit correctly, it is moved to crash.log.
// Returns path to crash.log or empty string.
// ponytail: single log for all instances, second instance running at the same time reports false crash, add QLockFile if needed
QString start_file_log()
{
	const QString dir = QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation) + QStringLiteral("/logs");
	if (!QDir().mkpath(dir)) {
		return {};
	}
	current_log_path = dir + QStringLiteral("/current.log");
	QString crash_log_path;
	if (QFile::exists(current_log_path)) {
		const QString path = dir + QStringLiteral("/crash.log");
		QFile::remove(path);
		if (QFile::rename(current_log_path, path)) {
			crash_log_path = path;
		}
	}
#ifdef Q_OS_WIN
	file_log = _wfopen(reinterpret_cast<const wchar_t *>(current_log_path.utf16()), L"wb");
#else
	file_log = std::fopen(QFile::encodeName(current_log_path).constData(), "wb");
#endif
	if (file_log) {
		file_log_old_message_handler = NecroLog::setMessageHandler(file_log_handler);
	}
	return crash_log_path;
}

void stop_file_log()
{
	if (file_log) {
		FILE *f = file_log;
		file_log = nullptr;
		std::fclose(f);
		QFile::remove(current_log_path);
	}
}

void copy_to_clipboard(const QString &text)
{
	QGuiApplication::clipboard()->setText(text);
#ifdef Q_OS_WIN
	// Qt renders clipboard data lazily, it would be lost if the app crashes again
	OleFlushClipboard();
#endif
}

// New GitHub issue prefilled with app info and as much of the log tail as fits into the URL
QUrl crash_report_issue_url(const QString &crash_log_path, const QString &log_tail)
{
	constexpr qsizetype max_query_size = 7000; // GitHub rejects URLs longer than ~8 KB
	auto commit_sha = QStringLiteral(COMMIT_SHA);
	if (commit_sha.isEmpty()) {
		commit_sha = QStringLiteral("N/A");
	}
	const QString title = QStringLiteral("QuickEvent did not exit correctly (ver. %1)").arg(QCoreApplication::applicationVersion());
	QStringList lines = log_tail.split('\n', Qt::SkipEmptyParts);
	lines = lines.mid(std::max<qsizetype>(0, lines.size() - 100));
	QByteArray query;
	for (;;) {
		const QString body = QStringLiteral(
			"## What happened\n"
			"<!-- Describe what you were doing when QuickEvent stopped working -->\n\n"
			"## Environment\n"
			"- QuickEvent: %1 (commit %2)\n"
			"- OS: %3 (%4)\n"
			"- Qt: %5\n\n"
			"## Log (last lines)\n"
			"```\n%6\n```\n\n"
			"Full log is in the clipboard (paste it here) or attach the file: `%7`\n")
			.arg(QCoreApplication::applicationVersion(), commit_sha, QSysInfo::prettyProductName(), QSysInfo::currentCpuArchitecture(), QString::fromLatin1(qVersion()), lines.join('\n'), QDir::toNativeSeparators(crash_log_path));
		query = "title=" + QUrl::toPercentEncoding(title) + "&body=" + QUrl::toPercentEncoding(body);
		if (query.size() <= max_query_size || lines.isEmpty()) {
			break;
		}
		lines.removeFirst();
	}
	return QUrl::fromEncoded("https://github.com/Quick-Box/quickevent/issues/new?" + query);
}

void show_crash_log_dialog(const QString &crash_log_path, QWidget *parent)
{
	QFile f(crash_log_path);
	if (!f.open(QFile::ReadOnly)) {
		return;
	}
	constexpr qint64 max_tail_size = 64 * 1024;
	if (f.size() > max_tail_size) {
		f.seek(f.size() - max_tail_size);
		f.readLine(); // skip partial line
	}
	const QString log_tail = QString::fromUtf8(f.readAll());

	const QString text = QCoreApplication::translate("main", "QuickEvent did not exit correctly last time.\n"
		"If you are going to report a problem, please attach the log.\n\n"
		"Log file: %1").arg(QDir::toNativeSeparators(crash_log_path));
	QMessageBox mbx(QMessageBox::Warning, QCoreApplication::translate("main", "QuickEvent"), text, QMessageBox::Close, parent);
	mbx.setDetailedText(log_tail);
	auto *copy_button = mbx.addButton(QCoreApplication::translate("main", "Copy log to clipboard"), QMessageBox::ActionRole);
	auto *open_dir_button = mbx.addButton(QCoreApplication::translate("main", "Open log folder"), QMessageBox::ActionRole);
	auto *report_button = mbx.addButton(QCoreApplication::translate("main", "Report issue on GitHub"), QMessageBox::ActionRole);
	mbx.exec();
	if (mbx.clickedButton() == copy_button) {
		copy_to_clipboard(log_tail);
	}
	else if (mbx.clickedButton() == open_dir_button) {
		QDesktopServices::openUrl(QUrl::fromLocalFile(QFileInfo(crash_log_path).absolutePath()));
	}
	else if (mbx.clickedButton() == report_button) {
		copy_to_clipboard(log_tail);
		QDesktopServices::openUrl(crash_report_issue_url(crash_log_path, log_tail));
	}
}

void qt_message_handler(QtMsgType type, const QMessageLogContext &context, const QString &msg)
{
	NecroLog::Level level = NecroLog::Level::Debug;
	switch (type) {
	case QtDebugMsg: level = NecroLog::Level::Debug; break;
	case QtInfoMsg: level = NecroLog::Level::Info; break;
	case QtWarningMsg: level = NecroLog::Level::Warning; break;
	case QtCriticalMsg: level = NecroLog::Level::Error; break;
	case QtFatalMsg: level = NecroLog::Level::Fatal; break;
	}
	const NecroLog::LogContext log_context(context.file, context.line, context.category);
	if (NecroLog::shouldLog(level, log_context)) {
		NecroLog::create(level, log_context) << msg.toStdString();
	}
}

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

	const QString crash_log_path = start_file_log();
	auto file_log_guard = qScopeGuard(stop_file_log);
	qInstallMessageHandler(qt_message_handler);

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
	qfInfo() << "Log file:" << (file_log ? current_log_path : QStringLiteral("N/A"));

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
	bool create_db_sql_script = false;
	foreach(QString s, cli_opts.unusedArguments()) {
		if (s == "--create-db-sql-script") {
			create_db_sql_script = true;
		} else {
			qfWarning() << "Undefined argument:" << s;
		}
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
	if (create_db_sql_script) {
		auto *event_plugin = qobject_cast<Event::EventPlugin*>(main_window.plugin("Event", qf::core::Exception::Throw));
		for (const auto &driver_name : {"SQLITE", "PSQL"}) {
			QString file_name = QStringLiteral("create_db_%1.sql").arg(QString(driver_name).toLower());
			QFile f(file_name);
			if (!f.open(QFile::WriteOnly)) {
				qfError() << "Cannot open file:" << f.fileName() << "for writing";
			} else {
				QFileInfo fi(f);
				qfInfo() << "Writing file:" << fi.canonicalFilePath();
				QVariantMap create_options;
				create_options["schemaName"] = "{{eventId}}";
				create_options["driverName"] = driver_name;
				auto lines = event_plugin->dbSchema()->createDbSqlScriptQml(create_options);
				QVariantMap replacements;
				replacements["minDbVersion"] = Event::EventPlugin::dbVersion();
				for (auto line : lines) {
					line = qf::core::Utils::replaceCaptions(line, replacements);
					f.write(line.toUtf8());
					f.write(";\n");
				}
			}
		}
		return EXIT_SUCCESS;
	}
	main_window.show();
	if (!crash_log_path.isEmpty()) {
		show_crash_log_dialog(crash_log_path, &main_window);
	}
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
