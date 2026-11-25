#pragma once

#include <qf/gui/framework/partwidget.h>

#include <QPointer>

class QLabel;
class QToolButton;

class PartWidget : public qf::gui::framework::PartWidget
{
	Q_OBJECT
private:
	typedef qf::gui::framework::PartWidget Super;
public:
	explicit PartWidget(const QString &title, const QString &feature_id, QWidget *parent = nullptr);

	Q_SIGNAL void resetPartRequest();
	Q_SIGNAL void reloadPartRequest();
protected:
	Q_SLOT virtual void onActiveChanged();
	void showEvent(QShowEvent *event) override;
private:
	void updateNoEventFrame();
private:
	QWidget *m_noEventFrame = nullptr;
	QLabel *m_noEventLabel = nullptr;
	QToolButton *m_btConnectDb = nullptr;
	QToolButton *m_btCreateEvent = nullptr;
	QToolButton *m_btOpenEvent = nullptr;
	QList<QPointer<QWidget>> m_hiddenByNoEvent;
};


