#pragma once

#include <QWidget>

class QTimer;

class CircularTimerWidget : public QWidget
{
	Q_OBJECT
	using Super = QWidget;
public:
	explicit CircularTimerWidget(QWidget *parent = nullptr);
	QSize sizeHint() const override;
	void setProgress(int remaining_ms, int total_ms);
	void markJustFired();
protected:
	void paintEvent(QPaintEvent *event) override;
private:
	int m_remaining_ms = -1;
	int m_total_ms = 0;
	bool m_just_fired = false;
	QTimer *m_firedFlashTimer = nullptr;
};
