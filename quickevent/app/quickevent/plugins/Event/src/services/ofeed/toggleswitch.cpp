#include "toggleswitch.h"

#include <QEasingCurve>
#include <QPainter>
#include <QPropertyAnimation>

ToggleSwitch::ToggleSwitch(QWidget *parent)
	: QAbstractButton(parent)
	, m_animation(new QPropertyAnimation(this, "handlePos", this))
{
	setCheckable(true);
	setCursor(Qt::PointingHandCursor);
	m_animation->setDuration(150);
	m_animation->setEasingCurve(QEasingCurve::InOutQuad);
	connect(this, &QAbstractButton::toggled, this, &ToggleSwitch::animate);
}

QSize ToggleSwitch::sizeHint() const
{
	return QSize(58, 26);
}

void ToggleSwitch::animate(bool checked)
{
	if(!isVisible()) {
		m_handlePos = checked ? 1.0 : 0.0;
		update();
		return;
	}
	m_animation->stop();
	m_animation->setStartValue(m_handlePos);
	m_animation->setEndValue(checked ? 1.0 : 0.0);
	m_animation->start();
}

void ToggleSwitch::paintEvent(QPaintEvent *)
{
	QPainter p(this);
	p.setRenderHint(QPainter::Antialiasing);

	const QRectF r = rect();
	const qreal h = r.height();
	const qreal w = r.width();
	const qreal handleD = h - 4.0;
	const qreal handleX = 2.0 + m_handlePos * (w - handleD - 4.0);

	// Track color interpolates: gray #9E9E9E → green #4CAF50
	const int cr = qRound(0x9E + m_handlePos * (0x4C - 0x9E));
	const int cg = qRound(0x9E + m_handlePos * (0xAF - 0x9E));
	const int cb = qRound(0x9E + m_handlePos * (0x50 - 0x9E));

	p.setPen(Qt::NoPen);
	p.setBrush(QColor(cr, cg, cb));
	p.drawRoundedRect(r, h / 2.0, h / 2.0);

	// "ON" label fades in after 30% of travel
	if (m_handlePos > 0.3) {
		const int alpha = qMin(255, qRound(255.0 * (m_handlePos - 0.3) / 0.7));
		QFont f = p.font();
		f.setPointSize(8);
		f.setBold(true);
		p.setFont(f);
		p.setPen(QColor(255, 255, 255, alpha));
		p.drawText(QRectF(6.0, 0.0, w / 2.0, h), Qt::AlignVCenter | Qt::AlignLeft, QStringLiteral("ON"));
	}

	// Handle drop shadow
	p.setPen(Qt::NoPen);
	p.setOpacity(0.15);
	p.setBrush(Qt::black);
	p.drawEllipse(QRectF(handleX + 1.0, 3.0, handleD, handleD));

	// Handle
	p.setOpacity(1.0);
	p.setBrush(Qt::white);
	p.drawEllipse(QRectF(handleX, 2.0, handleD, handleD));
}
