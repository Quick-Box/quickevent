#include "coreplugin.h"
#include "widgets/appstatusbar.h"
#include "widgets/settingsdialog.h"
#include "widgets/reportssettingspage.h"

#include <qf/qmlwidgets/framework/mainwindow.h>
#include <qf/qmlwidgets/action.h>
#include <qf/qmlwidgets/menubar.h>

#include <qf/core/sql/connection.h>
#include <qf/core/log.h>
#include <qf/core/utils/crypt.h>

#include <QCoreApplication>
#include <QProcess>
#include <QMessageBox>
#include <QSettings>
#include <QSslSocket>
#include <QActionGroup>

namespace qfw = qf::qmlwidgets;
namespace qff = qf::qmlwidgets::framework;

namespace Core {

CorePlugin::CorePlugin(QObject *parent)
	: Super("Core", parent)
{
	connect(this, &Plugin::installed, this, &CorePlugin::onInstalled);//, Qt::QueuedConnection);
}

SettingsDialog *CorePlugin::settingsDialog()
{
	if(!m_settingsDialog) {
		m_settingsDialog = new SettingsDialog(nullptr);
	}
	return m_settingsDialog;
}

QString CorePlugin::SETTINGS_PREFIX_APPLICATION_LOCALE_LANGUAGE()
{
	static const auto s = QStringLiteral("application/locale/language");
	return s;
}

namespace {
qf::core::utils::Crypt s_crypt(qf::core::utils::Crypt::createGenerator(16808, 11, 2147483647));
}

QByteArray CorePlugin::encrypt(const QByteArray &data, int min_length)
{
	return s_crypt.encrypt(data, min_length);
}

QByteArray CorePlugin::decrypt(const QByteArray &data)
{
	return s_crypt.decrypt(data);
}

void CorePlugin::onInstalled()
{
	qff::MainWindow *fwk = qff::MainWindow::frameWork();
	fwk->setStatusBar(new AppStatusBar());

	{
		auto *page = new ReportsSettingsPage();
		settingsDialog()->addPage(page);
		setReportsDir(page->reportsDirectoryFromSettings());
		qfInfo() << "Reports dir set to:" << effectiveReportsDir();
	}

	auto *a_file = fwk->menuBar()->actionForPath("file", true);
	a_file->setText(tr("&File"));

	auto *a_file_import = a_file->addMenuInto("import", tr("&Import"));
	a_file->addActionInto(a_file_import);

	auto *a_file_export = a_file->addMenuInto("export", tr("&Export"));
	a_file->addActionInto(a_file_export);

	a_file->addSeparatorInto();
	{
		auto *a = new qfw::Action(tr("&Settings"));
		connect(a, &qfw::Action::triggered, this, [this]() {
			settingsDialog()->exec();
		});
		a_file->addActionInto(a);
	}
	a_file->addSeparatorInto();
	{
		auto *a_quit = new qfw::Action(tr("&Quit"));
		//a->setShortcut("ctrl+L");
		connect(a_quit, &qfw::Action::triggered, QCoreApplication::instance(), &QCoreApplication::quit);
		a_file->addActionInto(a_quit);
	}

	auto *a_tools = fwk->menuBar()->actionForPath("tools", true);
	a_tools->setText(tr("&Tools"));
	{
		auto *a = new qfw::Action(tr("&SQL tool"));
		//a->setShortcut("ctrl+L");
		connect(a, &qfw::Action::triggered, this, &CorePlugin::launchSqlTool);
		a_tools->addActionInto(a);
	}
	{
		auto *m1 = a_tools->addMenuInto("locale", tr("&Locale"));
		{
			auto *m2 = m1->addMenuInto("language", tr("&Language"));
			QPair<QString, QString> langs[] = {
				{tr("System"), "system"},
				{tr("Czech"), "cs_CZ"},
				{tr("English"), "en_US"},
				{tr("Flemish"), "nl_BE"},
				{tr("French"), "fr_FR"},
				{tr("Norwegian"), "nb_NO"},
				{tr("Polish"), "pl_PL"},
				{tr("Russian"), "ru_RU"},
				{tr("Ukrainian"), "uk_UA"},
			};
			auto *ag = new QActionGroup(m2);
			QSettings settings;
			QString curr_lang = settings.value(SETTINGS_PREFIX_APPLICATION_LOCALE_LANGUAGE(), "system").toString();
			for(auto i : langs) {
				auto *a = new qfw::Action(i.first);
				ag->addAction(a);
				a->setOid(i.second);
				a->setCheckable(true);
				if(a->oid() == curr_lang)
					a->setChecked(true);
				m2->addActionInto(a);
				connect(a, &QAction::triggered, a, [a](bool checked) {
					if(checked) {
						QSettings settings;
						settings.setValue(SETTINGS_PREFIX_APPLICATION_LOCALE_LANGUAGE(), a->oid());
						QMessageBox::information(qff::MainWindow::frameWork()
												 , tr("Information")
												 , tr("Language change to '%1' will be applied after application restart.").arg(a->text()));
					}
				});
			}
		}
	}
	{
		auto *a = fwk->menuBar()->actionForPath("view", true);
		a->setText(tr("&View"));
	}
	{
		auto *a = fwk->menuBar()->actionForPath("view/toolbar", true);
		a->setText(tr("&Toolbar"));
	}
	{
		auto *a_help = fwk->menuBar()->actionForPath("help", true);
		a_help->setText(tr("&Help"));
		{
			auto *a = new qfw::Action(tr("&About Quick Event"));
			connect(a, &qfw::Action::triggered, this, &CorePlugin::aboutQuickEvent);
			a_help->addActionInto(a);
		}
		{
			auto *a = new qfw::Action(tr("About &Qt"));
			connect(a, &qfw::Action::triggered, this, &CorePlugin::aboutQt);
			a_help->addActionInto(a);
		}
	}
}

void CorePlugin::launchSqlTool()
{
	qfLogFuncFrame();
	QString program = QCoreApplication::applicationDirPath() + "/qsqlmon";
#ifdef Q_OS_WIN
	program += ".exe";
#endif
	qfDebug() << "launchnig" << program;
	QStringList otcs;
	{
		auto conn = qf::core::sql::Connection::forName();
		otcs << "description=QuickEvent";
		otcs << "driver=" + conn.driverName();
		otcs << "host=" + conn.hostName();
		otcs << "port=" + QString::number(conn.port());
		otcs << "user=" + conn.userName();
		otcs << "password=" + conn.password();
		otcs << "database=" + conn.databaseName();
	}

	QStringList arguments;
	arguments << "--one-time-connection-settings" << otcs.join('&');
	auto *process = new QProcess(this);
	process->start(program, arguments);
}

void CorePlugin::aboutQuickEvent()
{
	qff::MainWindow *fwk = qff::MainWindow::frameWork();
	QString version_string;
	QMetaObject::invokeMethod(fwk, "versionString", Qt::DirectConnection
							  , Q_RETURN_ARG(QString, version_string));
	QString db_version_string;
	QMetaObject::invokeMethod(fwk, "dbVersionString", Qt::DirectConnection
							  , Q_RETURN_ARG(QString, db_version_string));

	auto commit_sha = QStringLiteral(COMMIT_SHA);
	if (commit_sha.isEmpty()) {
		commit_sha = "N/A";
	}

	QMessageBox::about(fwk
					   , tr("About Quick Event")
					   , tr("The <b>Quick Event</b> is an application which helps you to organize the orienteering events."
							"<br/><br/>"
							"version: %1<br/>"
							"commit: %7<br/>"
							"min. db version: %2<br/>"
							"build: %3 %4<br/>"
							"SSL build: %5<br/>"
							"SSL run: %6"
							)
					   .arg(version_string)
					   .arg(db_version_string)
					   .arg(__DATE__).arg(__TIME__)
					   .arg(QSslSocket::sslLibraryBuildVersionString())
					   .arg(QSslSocket::sslLibraryVersionString())
					   .arg(commit_sha)
					   );
}

void CorePlugin::aboutQt()
{
	qff::MainWindow *fwk = qff::MainWindow::frameWork();
	QMessageBox::aboutQt(fwk , tr("About Qt"));
}

}
