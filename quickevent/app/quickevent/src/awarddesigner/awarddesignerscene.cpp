#include "awarddesignerscene.h"

#include <QPainter>
#include <QStyleOptionGraphicsItem>
#include <QGraphicsRectItem>
#include <QFileInfo>
#include <QGraphicsSceneMouseEvent>
#include <QGraphicsSceneHoverEvent>

// ─── AwardSceneItem ──────────────────────────────────────────────────────────

AwardSceneItem::AwardSceneItem(const AwardDesigner::Item &item, QGraphicsItem *parent)
	: QGraphicsObject(parent)
	, m_item(item)
{
	setFlags(ItemIsMovable | ItemIsSelectable | ItemSendsGeometryChanges);
	setAcceptHoverEvents(true);
	setPos(m_item.x * MM, m_item.y * MM);
	setZValue(m_item.zOrder);
}

void AwardSceneItem::refreshFromItem()
{
	prepareGeometryChange();
	setPos(m_item.x * MM, m_item.y * MM);
	setZValue(m_item.zOrder);
	if (m_item.kind == AwardDesigner::Item::Image) {
		m_pixmapLoaded = false;
	}
	update();
}

QRectF AwardSceneItem::boundingRect() const
{
	// Expand slightly to include handle squares that lie outside the item rect
	const qreal h2 = HANDLE_PX / 2.0;
	return QRectF(-h2, -h2, m_item.w * MM + HANDLE_PX, m_item.h * MM + HANDLE_PX);
}

// Returns the rect (in item coordinates) for one of the four corner handles
QRectF AwardSceneItem::handleRect(ResizeHandle h) const
{
	const qreal w = m_item.w * MM;
	const qreal ht = m_item.h * MM;
	const qreal s = HANDLE_PX;
	const qreal hs = s / 2.0;
	switch (h) {
	case ResizeHandle::TopLeft: return {-hs, -hs, s, s};
	case ResizeHandle::TopRight: return {w - hs, -hs, s, s};
	case ResizeHandle::BottomLeft: return {-hs, ht - hs, s, s};
	case ResizeHandle::BottomRight: return {w - hs, ht - hs, s, s};
	default: return {};
	}
}

AwardSceneItem::ResizeHandle AwardSceneItem::handleAt(const QPointF &item_pos) const
{
	if (!isSelected()) {
		return ResizeHandle::None;
	}
	static const ResizeHandle all[] = {
		ResizeHandle::TopLeft, ResizeHandle::TopRight,
		ResizeHandle::BottomLeft, ResizeHandle::BottomRight
	};
	for (auto h : all) {
		if (handleRect(h).contains(item_pos)) {
			return h;
		}
	}
	return ResizeHandle::None;
}

void AwardSceneItem::applyResize(const QPointF &scene_pos)
{
	const QPointF delta = scene_pos - m_resizeStartScene;
	const qreal dx = delta.x() / MM;
	const qreal dy = delta.y() / MM;

	qreal new_x = m_origItem.x;
	qreal new_y = m_origItem.y;
	qreal new_w;
	qreal new_h;

	switch (m_activeHandle) {
	case ResizeHandle::BottomRight:
		new_w = m_origItem.w + dx;
		new_h = m_origItem.h + dy;
		break;
	case ResizeHandle::BottomLeft:
		new_x = m_origItem.x + dx;
		new_w = m_origItem.w - dx;
		new_h = m_origItem.h + dy;
		break;
	case ResizeHandle::TopRight:
		new_y = m_origItem.y + dy;
		new_w = m_origItem.w + dx;
		new_h = m_origItem.h - dy;
		break;
	case ResizeHandle::TopLeft:
		new_x = m_origItem.x + dx;
		new_y = m_origItem.y + dy;
		new_w = m_origItem.w - dx;
		new_h = m_origItem.h - dy;
		break;
	default:
		return;
	}

	// Clamp minimums
	static constexpr qreal MIN_W = 5.0;
	static constexpr qreal MIN_H = 3.0;
	if (new_w < MIN_W) {
		if (m_activeHandle == ResizeHandle::BottomLeft || m_activeHandle == ResizeHandle::TopLeft) {
			new_x = m_origItem.x + m_origItem.w - MIN_W;
		}
		new_w = MIN_W;
	}
	if (new_h < MIN_H) {
		if (m_activeHandle == ResizeHandle::TopLeft || m_activeHandle == ResizeHandle::TopRight) {
			new_y = m_origItem.y + m_origItem.h - MIN_H;
		}
		new_h = MIN_H;
	}

	// Proportional scaling: scale the other dimension off the changed dimension
	if (m_item.scaleProportional && m_origItem.h > 0 && m_origItem.w > 0) {
		const qreal aspect = m_origItem.w / m_origItem.h;
		// Decide dominant axis: whichever changed more relatively
		const qreal rel_w = qAbs(new_w - m_origItem.w) / m_origItem.w;
		const qreal rel_h = qAbs(new_h - m_origItem.h) / m_origItem.h;
		if (rel_w >= rel_h) {
			const qreal old_h = new_h;
			new_h = qMax(MIN_H, new_w / aspect);
			// Adjust anchor for top-side handles
			if (m_activeHandle == ResizeHandle::TopLeft || m_activeHandle == ResizeHandle::TopRight) {
				new_y = m_origItem.y + (m_origItem.h - new_h);
			}
			Q_UNUSED(old_h)
		} else {
			new_w = qMax(MIN_W, new_h * aspect);
			if (m_activeHandle == ResizeHandle::BottomLeft || m_activeHandle == ResizeHandle::TopLeft) {
				new_x = m_origItem.x + (m_origItem.w - new_w);
			}
		}
	}

	prepareGeometryChange();
	m_item.x = new_x;
	m_item.y = new_y;
	m_item.w = new_w;
	m_item.h = new_h;
	setPos(m_item.x * MM, m_item.y * MM);
	update();
}

void AwardSceneItem::paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *)
{
	const qreal w = m_item.w * MM;
	const qreal h = m_item.h * MM;
	const QRectF r(0, 0, w, h);

	if (m_item.kind == AwardDesigner::Item::Image) {
		ensurePixmap();
		if (!m_pixmap.isNull()) {
			if (m_item.scaleProportional) {
				painter->drawPixmap(r.toRect(),
					m_pixmap.scaled(r.toRect().size(), Qt::KeepAspectRatio, Qt::SmoothTransformation));
			} else {
				painter->drawPixmap(r.toRect(), m_pixmap);
			}
		} else {
			painter->fillRect(r, QColor(220, 220, 220));
			painter->setPen(Qt::darkGray);
			painter->drawRect(r.adjusted(0.5, 0.5, -0.5, -0.5));
			painter->drawText(r, Qt::AlignCenter,
				QStringLiteral("[ Obrázek ]\n") + QFileInfo(m_item.imagePath).fileName());
		}
	} else {
		painter->fillRect(r, QColor(240, 248, 255, 200));
		painter->setPen(QColor(160, 200, 230));
		painter->drawRect(r.adjusted(0.5, 0.5, -0.5, -0.5));

		QFont font(m_item.fontFamily, m_item.fontSize);
		font.setBold(m_item.bold);
		font.setItalic(m_item.italic);
		painter->setFont(font);
		painter->setPen(QColor(m_item.color));

		QString label;
		if (m_item.fieldId == QLatin1String("customText")) {
			label = m_item.customText.isEmpty() ? QStringLiteral("[vlastní text]") : m_item.customText;
		} else {
			label = fieldLabel(m_item.fieldId);
		}

		Qt::Alignment align = static_cast<Qt::Alignment>(m_item.halign) | Qt::AlignVCenter;
		painter->drawText(r.adjusted(3, 2, -3, -2), align, label);
	}

	if (option->state & QStyle::State_Selected) {
		QPen sel_pen(QColor(30, 120, 255), 1.5, Qt::DashLine);
		painter->setPen(sel_pen);
		painter->setBrush(Qt::NoBrush);
		painter->drawRect(r.adjusted(0.75, 0.75, -0.75, -0.75));

		// Corner handles
		painter->setPen(QPen(QColor(30, 120, 255), 1.0));
		painter->setBrush(QColor(255, 255, 255, 220));
		static const ResizeHandle corners[] = {
			ResizeHandle::TopLeft, ResizeHandle::TopRight,
			ResizeHandle::BottomLeft, ResizeHandle::BottomRight
		};
		for (auto hd : corners) {
			painter->drawRect(handleRect(hd));
		}
	}
}

void AwardSceneItem::mousePressEvent(QGraphicsSceneMouseEvent *event)
{
	if (event->button() == Qt::LeftButton && isSelected()) {
		ResizeHandle h = handleAt(event->pos());
		if (h != ResizeHandle::None) {
			m_resizing = true;
			m_activeHandle = h;
			m_resizeStartScene = event->scenePos();
			m_origItem = m_item;
			setFlag(ItemIsMovable, false);
			event->accept();
			return;
		}
	}
	QGraphicsObject::mousePressEvent(event);
}

void AwardSceneItem::mouseMoveEvent(QGraphicsSceneMouseEvent *event)
{
	if (m_resizing) {
		applyResize(event->scenePos());
		emit geometryChanged();
		event->accept();
		return;
	}
	QGraphicsObject::mouseMoveEvent(event);
}

void AwardSceneItem::mouseReleaseEvent(QGraphicsSceneMouseEvent *event)
{
	if (m_resizing) {
		m_resizing = false;
		m_activeHandle = ResizeHandle::None;
		setFlag(ItemIsMovable, true);
		emit geometryChanged();
		event->accept();
		return;
	}
	QGraphicsObject::mouseReleaseEvent(event);
}

void AwardSceneItem::hoverMoveEvent(QGraphicsSceneHoverEvent *event)
{
	if (isSelected()) {
		ResizeHandle h = handleAt(event->pos());
		switch (h) {
		case ResizeHandle::TopLeft:
		case ResizeHandle::BottomRight:
			setCursor(Qt::SizeFDiagCursor);
			break;
		case ResizeHandle::TopRight:
		case ResizeHandle::BottomLeft:
			setCursor(Qt::SizeBDiagCursor);
			break;
		default:
			unsetCursor();
			break;
		}
	} else {
		unsetCursor();
	}
	QGraphicsObject::hoverMoveEvent(event);
}

void AwardSceneItem::hoverLeaveEvent(QGraphicsSceneHoverEvent *event)
{
	unsetCursor();
	QGraphicsObject::hoverLeaveEvent(event);
}

QVariant AwardSceneItem::itemChange(GraphicsItemChange change, const QVariant &value)
{
	if (change == ItemPositionHasChanged && scene() && !m_resizing) {
		QPointF pos = value.toPointF();
		m_item.x = pos.x() / MM;
		m_item.y = pos.y() / MM;
		emit geometryChanged();
	}
	return QGraphicsObject::itemChange(change, value);
}

void AwardSceneItem::ensurePixmap()
{
	if (m_pixmapLoaded) {
		return;
	}
	m_pixmapLoaded = true;
	if (!m_item.imageData.isEmpty()) {
		m_pixmap.loadFromData(m_item.imageData);
	} else if (!m_item.imagePath.isEmpty()) {
		m_pixmap.load(m_item.imagePath);
	}
}

QString AwardSceneItem::fieldLabel(const QString &field_id) const
{
	const auto *s = qobject_cast<const AwardDesignerScene *>(scene());
	const QList<AwardDesigner::FieldDef> &fields = s
		? s->availableFields()
		: AwardDesigner::relayFields();
	for (const auto &fd : fields) {
		if (fd.id == field_id) {
			return QStringLiteral("[") + fd.label + QStringLiteral("]");
		}
	}
	return QStringLiteral("[") + field_id + QStringLiteral("]");
}

// ─── AwardDesignerScene ───────────────────────────────────────────────────────

AwardDesignerScene::AwardDesignerScene(QObject *parent)
	: QGraphicsScene(parent)
{
	setSceneRect(0, 0, m_pageW * MM, m_pageH * MM);

	m_pageRect = addRect(0, 0, m_pageW * MM, m_pageH * MM,
		QPen(Qt::lightGray), QBrush(Qt::white));
	m_pageRect->setZValue(-1);

	connect(this, &QGraphicsScene::selectionChanged, this, &AwardDesignerScene::onSelectionChanged);
}

AwardDesignerScene::~AwardDesignerScene()
{
	// QGraphicsScene::~QGraphicsScene() clears its items and may emit
	// selectionChanged after the derived destructor has started. Avoid dispatching
	// to a derived-class member while the base destructor is running.
	disconnect(this, &QGraphicsScene::selectionChanged, this, &AwardDesignerScene::onSelectionChanged);
}

void AwardDesignerScene::setAvailableFields(const QList<AwardDesigner::FieldDef> &fields)
{
	m_availableFields = fields;
	for (auto *item : m_items) {
		item->update();
	}
}

void AwardDesignerScene::loadDesign(const AwardDesigner::Design &design)
{
	for (auto *item : m_items) {
		removeItem(item);
	}
	qDeleteAll(m_items);
	m_items.clear();

	m_pageW = design.pageW;
	m_pageH = design.pageH;
	setSceneRect(0, 0, m_pageW * MM, m_pageH * MM);
	m_pageRect->setRect(0, 0, m_pageW * MM, m_pageH * MM);

	for (const auto &item : design.items) {
		addDesignItem(item);
	}
}

AwardDesigner::Design AwardDesignerScene::collectDesign(const QString &name) const
{
	AwardDesigner::Design d;
	d.name = name;
	d.pageW = m_pageW;
	d.pageH = m_pageH;
	for (const auto *si : m_items) {
		d.items.append(si->designItem());
	}
	return d;
}

void AwardDesignerScene::addDesignItem(const AwardDesigner::Item &item)
{
	auto *si = new AwardSceneItem(item);
	addItem(si);
	m_items.append(si);
	connect(si, &AwardSceneItem::geometryChanged, this, [this, si]() {
		emit selectedItemChanged(si);
	});
}

void AwardDesignerScene::deleteSelected()
{
	AwardSceneItem *sel = selectedSceneItem();
	if (!sel) {
		return;
	}
	m_items.removeOne(sel);
	removeItem(sel);
	delete sel;
	emit selectedItemChanged(nullptr);
}

AwardSceneItem *AwardDesignerScene::selectedSceneItem() const
{
	auto sel = selectedItems();
	if (sel.size() == 1) {
		return dynamic_cast<AwardSceneItem *>(sel.first());
	}
	return nullptr;
}

void AwardDesignerScene::keyPressEvent(QKeyEvent *event)
{
	if (event->key() == Qt::Key_Delete) {
		deleteSelected();
		event->accept();
		return;
	}
	QGraphicsScene::keyPressEvent(event);
}

void AwardDesignerScene::onSelectionChanged()
{
	emit selectedItemChanged(selectedSceneItem());
}
