#ifndef QF_GUI_REPORTS_STYLE_SHEET_H
#define QF_GUI_REPORTS_STYLE_SHEET_H

#include "color.h"
#include "pen.h"
#include "brush.h"
#include "font.h"
#include "text.h"

#include "../../../guiglobal.h"

#include <qf/core/utils.h>

#include <QObject>
#include <QString>
#include <QBrush>
#include <QFont>
#include <QPen>

namespace qf {
namespace gui {
namespace reports {
namespace style {

class QFGUI_DECL_EXPORT Sheet : public QObject
{
	Q_OBJECT
	/// set name of prototype or prototype itself here
	Q_PROPERTY(QVariant basedOn READ basedOn WRITE setBasedOn)

	QF_PROPERTY_IMPL(QVariant, b, B, asedOn)
public:
	explicit Sheet(QObject *parent = nullptr);
	~Sheet() Q_DECL_OVERRIDE;

	void createStyleCache();
	QObject* styleObjectForName(StyleObject::StyleGroup style_object_group, const QString &name, bool should_exist = true);
	void setStyleObjectForName(StyleObject::StyleGroup style_object_group, const QString &name, QObject *o);

	/// style objects are looked up by name among sheet children, set sheet as their parent
	QList<Color*>& colors() {return m_colors;}
	QList<Pen*>& pens() {return m_pens;}
	QList<Brush*>& brushes() {return m_brushes;}
	QList<Font*>& fonts() {return m_fonts;}
	QList<Text*>& textStyles() {return m_textStyles;}
private:
	void createStyleCache_helper(QObject *parent);
private:
	QList<Color*> m_colors;
	QList<Pen*> m_pens;
	QList<Brush*> m_brushes;
	QList<Font*> m_fonts;
	QList<Text*> m_textStyles;
	typedef QMap<QString, QObject*> ObjectMap;
	QMap<StyleObject::StyleGroup, ObjectMap> m_definedStyles;
};

}}}}

#endif // QF_GUI_REPORTS_STYLE_SHEET_H
