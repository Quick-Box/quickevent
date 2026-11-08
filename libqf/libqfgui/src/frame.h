#ifndef QF_GUI_FRAME_H
#define QF_GUI_FRAME_H

#include "guiglobal.h"
#include "layouttypeproperties.h"

#include <qf/core/utils.h>

#include <QFrame>

class QBoxLayout;

namespace qf {
namespace gui {

class QFGUI_DECL_EXPORT Frame : public QFrame
{
	Q_OBJECT
	Q_PROPERTY(LayoutType layoutType READ layoutType WRITE setLayoutType NOTIFY layoutTypeChanged)
	Q_PROPERTY(qf::gui::LayoutTypeProperties* layoutProperties READ layoutTypeProperties WRITE setLayoutTypeProperties)
	Q_PROPERTY(bool visible READ isVisible WRITE setVisible NOTIFY visibleChanged)
private:
	typedef QFrame Super;
public:
	enum LayoutType {LayoutInvalid, LayoutHorizontal, LayoutVertical, LayoutGrid, LayoutForm};
	Q_ENUM(LayoutType)
public:
	explicit Frame(QWidget *parent = nullptr);
	~Frame() Q_DECL_OVERRIDE;
public:
	LayoutType layoutType() const;
	void setLayoutType(LayoutType ly);
	Q_SIGNAL void layoutTypeChanged(LayoutType ly);
	void addWidget(QWidget *control) {add(control);}

	void setVisible(bool b) Q_DECL_OVERRIDE;
	Q_SIGNAL void visibleChanged(bool visible);
private:
	void add(QWidget *control);

	LayoutTypeProperties* layoutTypeProperties() {return m_layoutTypeProperties;}
	void setLayoutTypeProperties(LayoutTypeProperties *props);

	void addToLayout(QWidget *widget);
	void createLayout(LayoutType layout_type);

private:
	LayoutType m_layoutType;
	LayoutTypeProperties *m_layoutTypeProperties;
	QList<QWidget*> m_childWidgets;
	int m_currentLayoutRow;
	int m_currentLayoutColumn;
};

}
}

#endif // FRAME_H
