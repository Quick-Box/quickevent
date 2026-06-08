#pragma once

#include <QAbstractButton>

class QPropertyAnimation;

class ToggleSwitch : public QAbstractButton
{
	Q_OBJECT
	Q_PROPERTY(qreal handlePos READ handlePos WRITE setHandlePos)

	using Super = QAbstractButton;
public:
	explicit ToggleSwitch(QWidget *parent = nullptr);

	QSize sizeHint() const override;

protected:
	void paintEvent(QPaintEvent *event) override;

private:
	qreal handlePos() const { return m_handlePos; }
	void setHandlePos(qreal pos) { m_handlePos = pos; update(); }
	void animate(bool checked);

	qreal m_handlePos = 0.0;
	QPropertyAnimation *m_animation;
};
