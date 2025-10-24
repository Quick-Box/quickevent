#include "mainwindow.h"
#include "loggerwidget.h"

#include <qf/gui/framework/stackedcentralwidget.h>
#include <qf/gui/framework/partswitch.h>
#include <qf/gui/framework/dockwidget.h>
#include <qf/gui/menubar.h>
#include <qf/gui/action.h>
#include <plugins/Core/src/coreplugin.h>
#include <plugins/Event/src/eventplugin.h>
#include <plugins/Classes/src/classesplugin.h>
#include <plugins/Runs/src/runsplugin.h>
#include <plugins/Oris/src/orisplugin.h>
#include <plugins/CardReader/src/cardreaderplugin.h>
#include <plugins/Receipts/src/receiptsplugin.h>
#include <plugins/Relays/src/relaysplugin.h>
#include <plugins/Speaker/src/speakerplugin.h>

#include <QCoreApplication>

MainWindow::MainWindow(QWidget *parent, Qt::WindowFlags flags) :
	Super(parent, flags)
{
	setWindowTitle(tr("Quick Event ver. %1").arg(QCoreApplication::applicationVersion()));
	setWindowIcon(QIcon(":/quickevent/images/quickevent64.png"));
}

MainWindow::~MainWindow() = default;

void MainWindow::onPluginsLoaded()
{
	{
		auto *dw = new qf::gui::framework::DockWidget(nullptr);
		dw->setObjectName("loggingDockWidget");
		dw->setWindowTitle(tr("Application log"));
		addDockWidget(Qt::BottomDockWidgetArea, dw);
		auto *w = new LoggerWidget();
		connect(dw, &qf::gui::framework::DockWidget::visibilityChanged, w, &LoggerWidget::onDockWidgetVisibleChanged);
		dw->setWidget(w);
		dw->hide();
		auto *a = dw->toggleViewAction();
		//a->setCheckable(true);
		a->setShortcut(QKeySequence("ctrl+shift+L"));
		menuBar()->actionForPath("view")->addActionInto(a);
	}

	auto *w = qobject_cast<qf::gui::framework::StackedCentralWidget*>(centralWidget());
	menuBar()->actionForPath("view/toolbar")->addActionInto(w->partSwitch()->toggleViewAction());

	centralWidget()->setActivePart("Runs", true);
	setPersistentSettingsId("MainWindow");
	loadPersistentSettings();
}

void MainWindow::loadPlugins()
{
	{
		auto *plugin = new Core::CorePlugin(this);
		registerPlugin(plugin);
	}
	{
		auto *plugin = new Event::EventPlugin(this);
		registerPlugin(plugin);
	}
	// {
	// 	auto *plugin = new Competitors::CompetitorsPlugin(this);
	// 	registerPlugin(plugin);
	// }
	{
		auto *plugin = new Classes::ClassesPlugin(this);
		registerPlugin(plugin);
	}
	{
		auto *plugin = new Runs::RunsPlugin(this);
		registerPlugin(plugin);
	}
	{
		auto *plugin = new Oris::OrisPlugin(this);
		registerPlugin(plugin);
	}
	{
		auto *plugin = new CardReader::CardReaderPlugin(this);
		registerPlugin(plugin);
	}
	{
		auto *plugin = new Receipts::ReceiptsPlugin(this);
		registerPlugin(plugin);
	}
	{
		auto *plugin = new Relays::RelaysPlugin(this);
		registerPlugin(plugin);
	}
	{
		auto *plugin = new Speaker::SpeakerPlugin(this);
		registerPlugin(plugin);
	}
	Super::loadPlugins();
}
