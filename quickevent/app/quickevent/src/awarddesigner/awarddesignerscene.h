#pragma once
#include "awarddesign.h"

#include <QGraphicsObject>
#include <QGraphicsScene>
#include <QGraphicsSceneHoverEvent>
#include <QGraphicsSceneMouseEvent>
#include <QKeyEvent>

class QGraphicsRectItem;

class AwardSceneItem : public QGraphicsObject
{
	Q_OBJECT
public:
	static constexpr qreal MM = 3.7795275591;
	static constexpr qreal HANDLE_PX = 7.0; // handle square size in scene pixels

	explicit AwardSceneItem(const AwardDesigner::Item &item, QGraphicsItem *parent = nullptr);

	const AwardDesigner::Item &designItem() const { return m_item; }
	AwardDesigner::Item &designItem() { return m_item; }

	void refreshFromItem();

	QRectF boundingRect() const override;
	void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget) override;

Q_SIGNALS:
	void geometryChanged();

protected:
	QVariant itemChange(GraphicsItemChange change, const QVariant &value) override;
	void mousePressEvent(QGraphicsSceneMouseEvent *event) override;
	void mouseMoveEvent(QGraphicsSceneMouseEvent *event) override;
	void mouseReleaseEvent(QGraphicsSceneMouseEvent *event) override;
	void hoverMoveEvent(QGraphicsSceneHoverEvent *event) override;
	void hoverLeaveEvent(QGraphicsSceneHoverEvent *event) override;

private:
	enum class ResizeHandle { None = -1, TopLeft, TopRight, BottomLeft, BottomRight };

	AwardDesigner::Item m_item;
	QPixmap m_pixmap;
	bool m_pixmapLoaded = false;

	// Resize state
	bool m_resizing = false;
	ResizeHandle m_activeHandle = ResizeHandle::None;
	QPointF m_resizeStartScene;
	AwardDesigner::Item m_origItem;

	QRectF handleRect(ResizeHandle h) const;
	ResizeHandle handleAt(const QPointF &item_pos) const;
	void applyResize(const QPointF &scene_pos);

	void ensurePixmap();
	QString fieldLabel(const QString &field_id) const;
};

class AwardDesignerScene : public QGraphicsScene
{
	Q_OBJECT
public:
	explicit AwardDesignerScene(QObject *parent = nullptr);
	~AwardDesignerScene() override;

	void loadDesign(const AwardDesigner::Design &design);
	AwardDesigner::Design collectDesign(const QString &name = QString()) const;

	void setAvailableFields(const QList<AwardDesigner::FieldDef> &fields);
	const QList<AwardDesigner::FieldDef> &availableFields() const { return m_availableFields; }

	void addDesignItem(const AwardDesigner::Item &item);
	void deleteSelected();

	AwardSceneItem *selectedSceneItem() const;

Q_SIGNALS:
	void selectedItemChanged(AwardSceneItem *item); // nullptr when none/multi

protected:
	void keyPressEvent(QKeyEvent *event) override;

private Q_SLOTS:
	void onSelectionChanged();

private:
	static constexpr qreal MM = 3.7795275591;
	qreal m_pageW = 210;
	qreal m_pageH = 297;
	QGraphicsRectItem *m_pageRect = nullptr;
	QList<AwardSceneItem *> m_items;
	QList<AwardDesigner::FieldDef> m_availableFields;
};
