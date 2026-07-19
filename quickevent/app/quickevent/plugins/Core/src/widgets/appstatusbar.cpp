#include "appstatusbar.h"

#include <QProgressBar>
#include <QPushButton>
#include <QLabel>

namespace Core {

AppStatusBar::AppStatusBar(QWidget *parent)
	: Super(parent)
{
	setStyleSheet("background-color: rgb(118, 118, 118);");
	setSizeGripEnabled(true);

	m_lblEvent = new QLabel("---", this);
	m_lblEvent->setFrameShape(QFrame::Panel);
	m_lblEvent->setFrameShadow(QFrame::Plain);
	addPermanentWidget(m_lblEvent);

	m_btCurrentStage = new QPushButton(this);
	m_btCurrentStage->setToolTip(tr("Set current stage"));
	m_btCurrentStage->setStyleSheet("background: lime;");
	connect(m_btCurrentStage, &QPushButton::clicked, this, &AppStatusBar::stageClicked);
	addPermanentWidget(m_btCurrentStage);

	m_progress = new QProgressBar(this);
	m_progress->setMinimumWidth(200);
	m_progress->setMaximumWidth(200);
	addWidget(m_progress);

	m_lblMessage = new QLabel(this);
	m_lblMessage->setFrameShape(QFrame::Panel);
	m_lblMessage->setFrameShadow(QFrame::Plain);
	addWidget(m_lblMessage, 1);

	showProgress(QString(), 0, 0);
}

AppStatusBar::~AppStatusBar()
= default;

void AppStatusBar::setEventName(const QString &event_name)
{
	m_eventName = event_name;
	m_lblEvent->setText(event_name);
}

void AppStatusBar::setStageNo(int stage_no)
{
	m_stageNo = stage_no;
	m_btCurrentStage->setText(tr("E%1").arg(m_stageNo));
}

void AppStatusBar::showProgress(const QString &msg, int completed, int total)
{
	m_progress->setVisible(total > 0 && completed < total);
	m_lblMessage->setVisible(m_progress->isVisible());
	m_progress->setValue(completed);
	m_progress->setMaximum(total);
	QString message = msg;
	if(message.isEmpty() && total > 0)
		message = QString::number(100 * completed / total) + '%';
	m_lblMessage->setText(message);
}

}
