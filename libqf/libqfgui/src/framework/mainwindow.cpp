#include "mainwindow.h"
#include "application.h"
#include "partwidget.h"
#include "stackedcentralwidget.h"
#include "plugin.h"
#include "../menubar.h"
#include "../statusbar.h"
#include "../toolbar.h"
#include "../dialogs/qmldialog.h"

#include <qf/core/log.h>
#include <qf/core/exception.h>
#include <qf/core/assert.h>
#include <qf/core/utils.h>

#include <QQmlEngine>
#include <QQmlContext>
#include <QSettings>
#include <QCloseEvent>
#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QJsonObject>
#include <QJsonArray>
#include <QDockWidget>

using namespace qf::gui::framework;

MainWindow* MainWindow::self = nullptr;

MainWindow::MainWindow(QWidget *parent, Qt::WindowFlags flags)
	: QMainWindow(parent, flags)
	, IPersistentSettings(this)
{
	qfLogFuncFrame();
	Q_ASSERT(self == nullptr);
	self = this;

	Application *app = Application::instance();
	app->m_frameWork = this;
	connect(this, &MainWindow::pluginsLoaded, this, &MainWindow::onPluginsLoaded);
}

MainWindow::~MainWindow()
{
	qfLogFuncFrame();
	savePersistentSettings();
}

void MainWindow::loadPersistentSettings()
{
	QString path = persistentSettingsPath();
	qfLogFuncFrame() << path;
	if(!path.isEmpty()) {
		QSettings settings;
		settings.beginGroup(path);
		restoreGeometry(settings.value("geometry").toByteArray());
		restoreState(settings.value("state").toByteArray());
	}
}

void MainWindow::savePersistentSettings()
{
	QString path = persistentSettingsPath();
	qfLogFuncFrame() << path;
	if(!path.isEmpty()) {
		QSettings settings;
		settings.beginGroup(path);
		settings.setValue("state", saveState());
		settings.setValue("geometry", saveGeometry());
	}
}

void MainWindow::loadPlugins() {
	emit pluginsLoaded();
}

QList<Plugin *> MainWindow::installedPlugins()
{
	return m_loadedPlugins;
}

void MainWindow::showProgress(const QString &msg, int completed, int total)
{
	qfLogFuncFrame() << msg << completed << total;
	QCoreApplication::processEvents();
	emit progress(msg, completed, total);
}

void MainWindow::hideProgress()
{
	showProgress(QString(), 0, 0);
}
#ifdef GET_RESOURCE_IN_FRAMEWORK
QNetworkAccessManager *manager = new QNetworkAccessManager();
QMap<int, QNetworkReply*> servedReplies;
QMap<int, QByteArray> servedRepliesData;

int MainWindow::getResource(const QUrl &url, bool show_progress)
{
	static int request_count = 0;
	QNetworkRequest rq;
	rq.setUrl(url);
	//rq.setRawHeader("User-Agent", "MyOwnBrowser 1.0");

	QNetworkReply *reply = manager->get(rq);
	if(show_progress) {
		showProgress(QString("%1/%2 %3").arg(0).arg(100).arg(reply->url().toString()), 0, 100);
	}
	int curr_request_id = ++request_count;
	servedReplies[curr_request_id] = reply;
	connect(reply, &QNetworkReply::readyRead, [curr_request_id]() {
		qfLogFuncFrame() << curr_request_id;
		QNetworkReply *rpl = servedReplies.value(curr_request_id);
		if(rpl) {
			QByteArray ba = rpl->readAll();
			qDebug() << rpl << ba;
			servedRepliesData[curr_request_id].append(ba);
		}
	});
	connect(reply, &QNetworkReply::downloadProgress, [show_progress, curr_request_id, this](qint64 _received, qint64 _total) {
		QNetworkReply *rpl = servedReplies.value(curr_request_id);
		if(rpl) {
			int completed = _received;
			int total = _total;
			if(total < 0) {
				total = 3 * _received;
				if(show_progress) {
					this->showProgress(QString("%1/%2 %3").arg(_received).arg(_total).arg(rpl->url().toString()), completed, total);
				}
			}
		}
	});
	connect(reply, &QNetworkReply::finished, [curr_request_id, this]() {
		QNetworkReply *rpl = servedReplies.value(curr_request_id);
		if(rpl) {
			if(rpl->error() == QNetworkReply::NoError) {
				QByteArray ba = servedRepliesData.value(curr_request_id);
				QString text = QString::fromUtf8(ba);
				emit getResourceContentFinished(curr_request_id, true, text);
			}
			else {
				emit getResourceContentFinished(curr_request_id, false, rpl->errorString());
			}
			rpl->deleteLater();
		}
	});
	/*
	connect(reply, &QNetworkReply::error, [](QNetworkReply::NetworkError code) {
		qfError() << "netwoork error";
	});
	*/
	//connect(reply, SIGNAL(sslErrors(QList<QSslError>)), this, SLOT(slotSslErrors(QList<QSslError>)));
	return curr_request_id;
}
#endif
MainWindow *MainWindow::frameWork()
{
	Q_ASSERT(self != nullptr);
	return self;
}

bool MainWindow::setActivePart(const QString &feature_id)
{
	int ix = centralWidget()->featureToIndex(feature_id);
	if(ix < 0)
		return false;
	return centralWidget()->setActivePart(ix, true);
}

void MainWindow::registerPlugin(qf::gui::framework::Plugin *plugin)
{
	m_loadedPlugins.append(plugin);
	emit plugin->installed();
}

void MainWindow::closeEvent(QCloseEvent *ev)
{
	emit aboutToClose();
	ev->accept();
}

void MainWindow::onPluginsLoaded()
{
	//centralWidget()->setActivePart(0, true);
}

void MainWindow::setPersistentSettingDomains(const QString &organization_domain, const QString &organization_name, const QString &application_name)
{
	QCoreApplication::setOrganizationDomain(organization_domain);
	QCoreApplication::setOrganizationName(organization_name);
	if(!application_name.isEmpty()) {
		QCoreApplication::setApplicationName(application_name);
	}
	//QSettings::setDefaultFormat(QSettings::IniFormat);
}

qf::gui::MenuBar *MainWindow::menuBar()
{
	QMenuBar *mb = Super::menuBar();
	auto *menu_bar = qobject_cast<MenuBar*>(mb);
	if(!menu_bar) {
		QF_SAFE_DELETE(mb);
		menu_bar = new MenuBar(this);
		Super::setMenuBar(menu_bar);
	}
	return menu_bar;
}

qf::gui::ToolBar *MainWindow::toolBar(const QString &name, bool create_if_not_exists)
{
	qf::gui::ToolBar *ret = m_toolBars.value(name);
	if(!ret && !create_if_not_exists)
		return nullptr;
	ret = new qf::gui::ToolBar(this);
	addToolBar(Qt::TopToolBarArea, ret);
	m_toolBars[name] = ret;
	return ret;
}

qf::gui::StatusBar *MainWindow::statusBar()
{
	QStatusBar *sb = Super::statusBar();
	auto *status_bar = qobject_cast<StatusBar*>(sb);
	if(!status_bar) {
		QF_SAFE_DELETE(sb)
		status_bar = new StatusBar(this);
		Super::setStatusBar(status_bar);
	}
	return status_bar;
}

void MainWindow::setStatusBar(qf::gui::StatusBar *sbar)
{
	qfLogFuncFrame() << sbar << "previous:" << Super::statusBar();
	sbar->setParent(nullptr);
	connect(this, SIGNAL(progress(QString,int,int)), sbar, SLOT(showProgress(QString,int,int)));
	Super::setStatusBar(sbar); /// deletes old status bar
	qfDebug() << Super::statusBar();
}

CentralWidget *MainWindow::centralWidget()
{
	QWidget *cw = Super::centralWidget();
	auto *central_widget = qobject_cast<CentralWidget*>(cw);
	if(!central_widget) {
		QF_SAFE_DELETE(cw)
		central_widget = new StackedCentralWidget(this);
		//central_widget->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::MinimumExpanding);
		//central_widget->setStyleSheet("background:rgb(67, 67, 67)");
		Super::setCentralWidget(central_widget);
	}
	return central_widget;
}

void MainWindow::setCentralWidget(CentralWidget *widget)
{
	qfLogFuncFrame() << widget;
	widget->setParent(nullptr);
	Super::setCentralWidget(widget);
}

void MainWindow::addDockWidget(Qt::DockWidgetArea area, QDockWidget *dockwidget)
{
	Q_ASSERT(dockwidget);
	dockwidget->setParent(nullptr);
	if(auto* ps = dynamic_cast<qf::gui::framework::IPersistentSettings*>(dockwidget)) {
		ps->loadPersistentSettingsRecursively();
	}
	Super::addDockWidget(area, dockwidget);
}

void MainWindow::addPartWidget(PartWidget *widget, const QString &feature_id)
{
	QF_ASSERT(widget != nullptr, "Widget is NULL", return);
	if(!feature_id.isEmpty()) {
		if(widget->featureId().isEmpty()) {
			qfDebug() << "setting" << widget << "featureId to:" << feature_id;
			widget->setFeatureId(feature_id);
		}
		else if(widget->featureId() != feature_id)
			qfWarning() << "different featureIds set:" << feature_id << "vs." << widget->featureId() << ", the second one will be used.";
	}
	if(widget->featureId().isEmpty())
		qfWarning() << widget << "adding part widget without featureId set can harm some default functionality.";
	//qfWarning() << widget->featureId() << widget;
	widget->loadPersistentSettingsRecursively();
	centralWidget()->addPartWidget(widget);
}

Plugin *MainWindow::plugin(const QString &feature_id, bool throw_exc)
{
	Plugin *ret = nullptr;
	for(auto* plugin: m_loadedPlugins)
	{
		if(plugin->featureId() == feature_id)
			ret = plugin;
	}
	if(!ret) {
		if(throw_exc)
			QF_EXCEPTION(tr("Plugin for feature id: '%1' is not installed!").arg(feature_id));
	}
	return ret;
}

Plugin *MainWindow::pluginForObject(QObject *qml_object)
{
	qfLogFuncFrame();
	Plugin *ret = nullptr;
	for(QObject *o = qml_object; o!=nullptr; o=o->parent()) {
		auto *pw = qobject_cast<PartWidget*>(o);
		qfDebug() << o << "->" << pw;
		if(pw) {
			QString id = pw->featureId();
			ret = plugin(id);
			break;
		}
	}
	if(!ret)
		qfWarning() << "Cannot find plugin of:" << qml_object;
	return ret;
}

qf::gui::dialogs::QmlDialog *MainWindow::createQmlDialog(QWidget *parent)
{
	if(parent == nullptr)
		parent = this;
	auto *ret = new qf::gui::dialogs::QmlDialog(parent);
	//Application *app = Application::instance();
	//QQmlEngine *qe = app->qmlEngine();
	//qe->setObjectOwnership(ret, QQmlEngine::JavaScriptOwnership);
	return ret;
}
