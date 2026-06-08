#include "circulartimerwidget.h"

#include <QPainter>
#include <QTimer>

CircularTimerWidget::CircularTimerWidget(QWidget *parent)
	: Super(parent)
{
	m_firedFlashTimer = new QTimer(this);
	m_firedFlashTimer->setSingleShot(true);
	m_firedFlashTimer->setInterval(2000);
	connect(m_firedFlashTimer, &QTimer::timeout, this, [this]() {
		m_just_fired = false;
		update();
	});
	setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
}

QSize CircularTimerWidget::sizeHint() const
{
	return {36, 36};
}

void CircularTimerWidget::setProgress(int remaining_ms, int total_ms)
{
	if(m_just_fired)
		return;
	m_remaining_ms = remaining_ms;
	m_total_ms = total_ms;
	update();
}

void CircularTimerWidget::markJustFired()
{
	m_just_fired = true;
	m_firedFlashTimer->start();
	update();
}

void CircularTimerWidget::paintEvent(QPaintEvent *)
{
	QPainter p(this);
	p.setRenderHint(QPainter::Antialiasing);

	const int size = qMin(width(), height());
	const int margin = 2;
	const int ring_w = 4;
	const QRect outer_rect(margin, (height() - size) / 2 + margin, size - 2 * margin, size - 2 * margin);

	// Same palette as ToggleSwitch: gray #9E9E9E (off) → green #4CAF50 (on)
	const QColor bg_color(245, 245, 245);
	const QColor track_color(0xE0, 0xE0, 0xE0);
	const QColor arc_color(0x4C, 0xAF, 0x50);
	const QColor text_color(0x55, 0x55, 0x55);
	const QColor done_color(0x4C, 0xAF, 0x50);

	p.setPen(Qt::NoPen);
	p.setBrush(bg_color);
	p.drawEllipse(outer_rect);

	const QRect ring_rect = outer_rect.adjusted(ring_w / 2, ring_w / 2, -(ring_w + 1) / 2, -(ring_w + 1) / 2);

	if(m_just_fired) {
		// Full green ring + white checkmark — mirrors the ToggleSwitch ON state
		p.setPen(Qt::NoPen);
		p.setBrush(done_color);
		p.drawEllipse(outer_rect);

		const int cx = outer_rect.center().x();
		const int cy = outer_rect.center().y();
		const int r = size / 6;
		QPen check_pen(Qt::white, 2.2, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);
		p.setPen(check_pen);
		QPolygonF mark;
		mark << QPointF(cx - r, cy + 0.1 * r)
			 << QPointF(cx - 0.2 * r, cy + r * 0.9)
			 << QPointF(cx + r, cy - r * 0.7);
		p.drawPolyline(mark);
		return;
	}

	// Track ring (light gray — like the switch OFF track)
	p.setPen(QPen(track_color, ring_w, Qt::SolidLine, Qt::FlatCap));
	p.setBrush(Qt::NoBrush);
	p.drawArc(ring_rect, 90 * 16, 360 * 16);

	if(m_total_ms > 0 && m_remaining_ms >= 0) {
		// Green arc showing remaining time — same green as switch ON
		const double fraction = static_cast<double>(m_remaining_ms) / m_total_ms;
		const int span = static_cast<int>(fraction * 360.0 * 16);
		if(span > 0) {
			p.setPen(QPen(arc_color, ring_w, Qt::SolidLine, Qt::FlatCap));
			p.drawArc(ring_rect, 90 * 16, span);
		}

		// Center text: remaining time
		const int remaining_sec = (m_remaining_ms + 999) / 1000;
		QString text;
		if(remaining_sec >= 3600)
			text = QString::number(remaining_sec / 3600) + QStringLiteral("h");
		else if(remaining_sec >= 60)
			text = QString::number(remaining_sec / 60) + QStringLiteral("m");
		else
			text = QString::number(remaining_sec);

		QFont f = p.font();
		f.setPixelSize(qMax(8, size / 4));
		f.setBold(true);
		p.setFont(f);
		p.setPen(text_color);
		p.drawText(outer_rect, Qt::AlignCenter, text);
	}
}
