#include "appclioptions.h"

AppCliOptions::AppCliOptions(QObject *parent)
	: Super(parent)
{
	addOption("locale").setType(QMetaType::QString).setNames("--locale").setComment(tr("Application locale")).setDefaultValue("system");
	addOption("app.fontScale").setType(QMetaType::Double).setNames("--font-scale").setComment(tr("Application font scale")).setDefaultValue(1);
}
