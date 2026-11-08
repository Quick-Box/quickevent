#ifndef QF_GUI_FRAMEWORK_MAINWINDOW_H
#define QF_GUI_FRAMEWORK_MAINWINDOW_H

#include "../guiglobal.h"
#include "ipersistentsettings.h"
#include "../statusbar.h"
#include "../toolbar.h"
#include "partwidget.h"
#include "plugin.h"
//#include "pluginloader.h"

#include <qf/core/utils.h>

#include <QMainWindow>
#include <QMap>

namespace qf {
namespace core {
class Settings;
}
}
namespace qf {
namespace gui {

class MenuBar;

namespace framework {

class Application;
class PluginLoader;
class DockWidget;
class CentralWidget;
typedef QList<qf::gui::framework::Plugin*> PluginList;

class QFGUI_DECL_EXPORT MainWindow : public QMainWindow, public IPersistentSettings
{
	Q_OBJECT
	Q_PROPERTY(qf::gui::MenuBar* menuBar READ menuBar)
	Q_PROPERTY(qf::gui::StatusBar* statusBar READ statusBar)
	Q_PROPERTY(QString persistentSettingsId READ persistentSettingsId WRITE setPersistentSettingsId)
	Q_PROPERTY(QString uiLanguageName READ uiLanguageName WRITE setUiLanguageName NOTIFY uiLanguageNameChanged)
private:
	typedef QMainWindow Super;
public:
	explicit MainWindow(QWidget * parent = nullptr, Qt::WindowFlags flags = Qt::WindowFlags());
	~MainWindow() Q_DECL_OVERRIDE;

	QF_PROPERTY_IMPL(QString, u, U, iLanguageName)
public:
	CentralWidget* centralWidget();
	void setCentralWidget(CentralWidget *widget);
public:
	void registerPlugin(qf::gui::framework::Plugin *plugin);
	virtual void loadPlugins();
	QList<qf::gui::framework::Plugin*> installedPlugins();

	/// framework API
	Q_INVOKABLE void setPersistentSettingDomains(const QString &organization_domain, const QString &organization_name, const QString &application_name = QString());

	MenuBar* menuBar();

	Q_INVOKABLE qf::gui::ToolBar* toolBar(const QString &name, bool create_if_not_exists = false);

	StatusBar* statusBar();
	Q_INVOKABLE void setStatusBar(qf::gui::StatusBar *sbar);

	Q_INVOKABLE void addDockWidget(Qt::DockWidgetArea area, QDockWidget *dockwidget);
	Q_INVOKABLE void addPartWidget(qf::gui::framework::PartWidget *widget, const QString &feature_id = QString());

	Q_INVOKABLE qf::gui::framework::Plugin* plugin(const QString &feature_id, bool throw_exc = false);
	Q_INVOKABLE qf::gui::framework::Plugin* pluginForObject(QObject *qml_object);

	Q_SLOT void loadPersistentSettings();

	Q_SLOT void showProgress(const QString &msg, int completed, int total);
	Q_SLOT void hideProgress();
	Q_SIGNAL void progress(const QString &msg, int completed, int total);

	/// emitted by plugin loader when all plugins are loaded
	Q_SIGNAL void pluginsLoaded();
	Q_SIGNAL void applicationLaunched();
	Q_SIGNAL void aboutToClose();
#ifdef GET_RESOURCE_IN_FRAMEWORK
	Q_INVOKABLE int getResource(const QUrl &url, bool show_progress = true);
	Q_SIGNAL void getResourceContentFinished(int request_id, bool ok, const QString &content);
#endif
	static MainWindow* frameWork();

	Q_SLOT bool setActivePart(const QString &feature_id);

	//Q_INVOKABLE QObject* obj_testing();
protected:
	PluginLoader *pluginLoader();
	void closeEvent(QCloseEvent *ev) Q_DECL_OVERRIDE;
	Q_SLOT virtual void onPluginsLoaded();
private:
	Q_SLOT void savePersistentSettings();
private:
	PluginList m_loadedPlugins;
	PluginLoader *m_pluginLoader = nullptr;
	QMap<QString, qf::gui::ToolBar*> m_toolBars;
	static MainWindow *self;
};

template<typename T>
static T* getPlugin()
{
	static_assert(std::is_base_of<qf::gui::framework::Plugin, T>::value, "given type is not Plugin");
	qf::gui::framework::MainWindow *fwk = qf::gui::framework::MainWindow::frameWork();
	T* ret = nullptr;
	for(auto *p : fwk->installedPlugins()) {
		ret = dynamic_cast<T*>(p);
		if(ret)
			break;
	}
	if(!ret) {
		QF_EXCEPTION(QString("Plugin ") + typeid(T).name() + " not installed!");
	}
	return ret;
}

template<typename Widget, typename PartWidget>
static std::pair<PartWidget*, Widget*> initPluginWidget(QString title, QString featureId)
{
	auto* widget = new Widget();
	auto *part_widget = new PartWidget(title, featureId);
	qf::gui::framework::MainWindow::frameWork()->addPartWidget(part_widget);
	part_widget->addWidget(widget);
	widget->settleDownInPartWidget(part_widget);
	return std::make_pair(part_widget, widget);
}

}}}

#endif // QF_GUI_FRAMEWORK_MAINWINDOW_H
