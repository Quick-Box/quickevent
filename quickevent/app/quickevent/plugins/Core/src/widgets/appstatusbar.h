#pragma once

#include <qf/core/utils.h>

#include <qf/gui/statusbar.h>

class QLabel;
class QProgressBar;
class QPushButton;

namespace Core {

class AppStatusBar : public qf::gui::StatusBar
{
	Q_OBJECT
private:
	typedef qf::gui::StatusBar Super;
public:
	AppStatusBar(QWidget *parent = nullptr);
	~AppStatusBar() Q_DECL_OVERRIDE;

	QString eventDbName() const;
	void setEventDbName(const QString &event_name);

	int stageNo() const {return m_stageNo;}
	void setStageNo(int stage_no);
	Q_SIGNAL void stageClicked();

	void showProgress(const QString &msg, int completed, int total) Q_DECL_OVERRIDE;
private:
	QLabel *m_lblMessage;
	QProgressBar *m_progress;
	QLabel *m_lblEvent;
	QPushButton *m_btCurrentStage;

	int m_stageNo = 0;
};

}
