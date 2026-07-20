#include "partwidget.h"
#include "plugins/Event/src/eventplugin.h"

#include <qf/core/assert.h>

#include <qf/gui/framework/mainwindow.h>
#include <qf/gui/framework/plugin.h>


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
