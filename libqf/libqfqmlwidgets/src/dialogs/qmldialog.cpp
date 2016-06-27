#include "qmldialog.h"
#include "../frame.h"
#include "../dialogbuttonbox.h"

#include <qf/core/log.h>
#include <qf/core/utils.h>

#include <QVBoxLayout>
#include <QSettings>

using namespace qf::qmlwidgets::dialogs;

QmlDialog::QmlDialog(QWidget *parent) :
	Super(parent)
{
	qfLogFuncFrame();
	//connect(this, &QmlDialog::finished, this, &QmlDialog::savePersistentSettings);
	m_centralFrame = new Frame(nullptr);
	setCentralWidget(m_centralFrame);
	updateLayout();
}

QmlDialog::~QmlDialog()
{
}

int QmlDialog::standardButtons()
{
	int ret = 0;
	if(m_dialogButtonBox) {
		ret = m_dialogButtonBox->standardButtons();
	}
	return ret;
}

qf::qmlwidgets::framework::DialogWidget *QmlDialog::dialogWidget() const
{
	return qobject_cast<framework::DialogWidget*>(m_centralFrame);
}

void QmlDialog::setDialogWidget(qf::qmlwidgets::framework::DialogWidget *w)
{
	QWidget *old_w = m_centralWidget;
	setCentralWidget(w);
	if(old_w != w) {
		m_centralFrame = w;
		emit dialogWidgetChanged(w);
	}
}

QQmlListProperty<QWidget> QmlDialog::widgets()
{
	return m_centralFrame->widgets();
}

void QmlDialog::classBegin()
{
	qfLogFuncFrame();
}

void QmlDialog::componentComplete()
{
	qfLogFuncFrame();
	//loadPersistentSettingsRecursively();
}



