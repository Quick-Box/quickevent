#include "thispartwidget.h"
#include "competitorswidget.h"

#include <qf/core/log.h>

#include <qf/qmlwidgets/frame.h>

ThisPartWidget::ThisPartWidget(QWidget *parent)
	: Super(parent)
{
	setPersistentSettingsId("Competitors");
	setTitle(tr("&Competitors"));

	CompetitorsWidget *w = new CompetitorsWidget();
	centralFrame()->addWidget(w);
	w->settleDownInPartWidget(this);

	//QMetaObject::invokeMethod(this, "lazyInit", Qt::QueuedConnection);
}

