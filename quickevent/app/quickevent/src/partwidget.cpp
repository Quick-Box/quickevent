#include "partwidget.h"
#include "plugins/Event/src/eventplugin.h"

#include <qf/core/assert.h>

#include <qf/gui/action.h>
#include <qf/gui/framework/mainwindow.h>
#include <qf/gui/framework/plugin.h>

#include <QBoxLayout>
#include <QLabel>
#include <QToolButton>


PartWidget::PartWidget(const QString& title, const QString &feature_id, QWidget *parent)
	: Super(feature_id, parent)
{
	setPersistentSettingsId(featureId());
	setTitle(title);

	connect(this, &PartWidget::activeChanged, this, &PartWidget::onActiveChanged);
	auto *event_plugin = qf::gui::framework::getPlugin<Event::EventPlugin>();
	connect(event_plugin, &Event::EventPlugin::currentStageIdChanged, this, &PartWidget::resetPartRequest);
	connect(event_plugin, &Event::EventPlugin::eventOpenChanged, this, &PartWidget::resetPartRequest);
	connect(event_plugin, &Event::EventPlugin::reloadDataRequest, this, &PartWidget::resetPartRequest);
	connect(event_plugin, &Event::EventPlugin::eventOpenChanged, this, &PartWidget::updateNoEventFrame);
	connect(event_plugin, &Event::EventPlugin::sqlServerConnectedChanged, this, &PartWidget::updateNoEventFrame);
}

void PartWidget::onActiveChanged()
{
	if(isActive()) {
		qf::gui::framework::MainWindow *fwk = qf::gui::framework::MainWindow::frameWork();
		QF_ASSERT(fwk != nullptr, "Invalid FrameWork", return);
		auto *event_plugin = qf::gui::framework::getPlugin<Event::EventPlugin>();
		bool sql_connected = event_plugin->isSqlServerConnected();
		QString event_name = event_plugin->eventDbName();
		if(sql_connected && !event_name.isEmpty()) {
			emit reloadPartRequest();
		}
	}
}

void PartWidget::showEvent(QShowEvent *event)
{
	// menu bar and toolbars are created lazily by plugins, so update on show too
	updateNoEventFrame();
	Super::showEvent(event);
}

void PartWidget::updateNoEventFrame()
{
	auto *event_plugin = qf::gui::framework::getPlugin<Event::EventPlugin>();
	auto *ly = qobject_cast<QBoxLayout*>(layout());
	QF_ASSERT(ly != nullptr, "wrong layout", return);
	if(!m_noEventFrame) {
		m_noEventFrame = new QWidget(this);
		auto *frame_ly = new QVBoxLayout(m_noEventFrame);
		frame_ly->addStretch();
		m_noEventLabel = new QLabel();
		m_noEventLabel->setAlignment(Qt::AlignCenter);
		m_noEventLabel->setWordWrap(true);
		frame_ly->addWidget(m_noEventLabel);
		auto *buttons_ly = new QHBoxLayout();
		buttons_ly->addStretch();
		auto create_button = [buttons_ly](QAction *action) {
			auto *bt = new QToolButton();
			bt->setDefaultAction(action);
			bt->setToolButtonStyle(Qt::ToolButtonTextOnly);
			buttons_ly->addWidget(bt);
			return bt;
		};
		m_btConnectDb = create_button(event_plugin->connectDbAction());
		m_btCreateEvent = create_button(event_plugin->createEventAction());
		m_btOpenEvent = create_button(event_plugin->openEventAction());
		buttons_ly->addStretch();
		frame_ly->addLayout(buttons_ly);
		frame_ly->addStretch();
		ly->addWidget(m_noEventFrame);
	}
	const bool is_connected = event_plugin->isSqlServerConnected();
	const bool is_open = event_plugin->isEventOpen();
	// item 0 is the caption frame, keep it visible
	for(int i = 1; i < ly->count(); ++i) {
		if(auto *w = ly->itemAt(i)->widget(); w && w != m_noEventFrame) {
			w->setVisible(is_open);
		}
	}
	m_noEventFrame->setVisible(!is_open);
	m_noEventLabel->setText(is_connected
		? tr("No event is open.\nCreate a new event or open an existing one to continue.")
		: tr("Data storage is not set.\nConnect to a database (working directory or SQL server) first, then create or open an event."));
	m_btConnectDb->setVisible(!is_connected);
	m_btCreateEvent->setVisible(is_connected);
	m_btOpenEvent->setVisible(is_connected);
}
