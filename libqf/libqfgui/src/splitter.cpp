#include "splitter.h"

#include <qf/core/log.h>

#include <QSettings>

using namespace qf::gui;

Splitter::Splitter(QWidget *parent) :
	QSplitter(parent), framework::IPersistentSettings(this)
{
}

Splitter::~Splitter()
{
	savePersistentSettings();
}

void Splitter::loadPersistentSettings()
{
	QString path = persistentSettingsPath();
	qfLogFuncFrame() << path;
	if(!path.isEmpty()) {
		QSettings settings;
		settings.beginGroup(path);
		QByteArray ba = settings.value("sizes").toByteArray();
		if(!ba.isEmpty()) {
			this->restoreState(ba);
		}
	}
}

void Splitter::savePersistentSettings()
{
	QString path = persistentSettingsPath();
	qfLogFuncFrame() << path;
	if(!path.isEmpty()) {
		QSettings settings;
		settings.beginGroup(path);
		settings.setValue("sizes", saveState());
	}
}

