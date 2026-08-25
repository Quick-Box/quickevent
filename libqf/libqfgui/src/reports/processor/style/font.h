#ifndef QF_GUI_REPORTS_STYLE_FONT_H
#define QF_GUI_REPORTS_STYLE_FONT_H

#include "styleobject.h"

#include "../../../guiglobal.h"

#include <qf/core/utils.h>

#include <QObject>
#include <QFont>

namespace qf {
namespace gui {
namespace reports {
namespace style {

class QFGUI_DECL_EXPORT Font : public StyleObject
{
	Q_OBJECT
	// there are some restrictions for assigning primitive types to default properties, see: pen.h
	Q_CLASSINFO("DefaultProperty", "basedOn")
	Q_PROPERTY(QVariant basedOn READ basedOn WRITE setBasedOn)
	Q_PROPERTY(QString family READ family WRITE setFamily NOTIFY familyChanged)
	Q_PROPERTY(FontStyle style READ style WRITE setStyle NOTIFY styleChanged)
	Q_PROPERTY(FontStyleHint hint READ hint WRITE setHint NOTIFY hintChanged)
	Q_PROPERTY(FontWeight weight READ weight WRITE setWeight NOTIFY weightChanged)
	Q_PROPERTY(qreal pointSize READ pointSize WRITE setPointSize NOTIFY pointSizeChanged)
	Q_PROPERTY(StrikeOut strikeOut READ strikeOut WRITE setStrikeOut NOTIFY strikeOutChanged)
public:
	explicit Font(QObject *parent = nullptr);
	~Font() Q_DECL_OVERRIDE;
public:
	enum FontStyle {
		StyleNormal = QFont::StyleNormal,
		StyleItalic = QFont::StyleItalic,
		StyleOblique = QFont::StyleOblique,
		StyleInherited
	};
	enum FontStyleHint {
		HintAnyStyle = QFont::AnyStyle,
		HintSansSerif = QFont::SansSerif,
		HintHelvetica = QFont::Helvetica,
		HintSerif = QFont::Serif,
		HintTimes = QFont::Times,
		HintTypeWriter = QFont::TypeWriter,
		HintCourier = QFont::Courier,
		HintOldEnglish = QFont::OldEnglish,
		HintDecorative = QFont::Decorative,
		HintMonospace = QFont::Monospace,
		HintFantasy = QFont::Fantasy,
		HintCursive = QFont::Cursive,
		HintSystem = QFont::System,
		HintInherited
	};
	enum FontWeight {
		WeightLight = QFont::Light,
		WeightNormal = QFont::Normal,
		WeightDemiBold = QFont::DemiBold,
		WeightBold = QFont::Bold,
		WeightBlack = QFont::Black,
		WeightInherited
	};
	enum StrikeOut {
		StrikeOutFalse = 0,
		StrikeOutTrue = 1,
		StrikeOutInherited
	};
	Q_ENUM(FontStyle)
	Q_ENUM(FontStyleHint)
	Q_ENUM(FontWeight)
	Q_ENUM(StrikeOut)

	QF_PROPERTY_IMPL(QVariant, b, B, asedOn)
	QF_PROPERTY_IMPL(QString, f, F, amily)
	QF_PROPERTY_IMPL2(FontStyle, s, S, tyle, StyleInherited)
	QF_PROPERTY_IMPL2(FontStyleHint, h, H, int, HintInherited)
	QF_PROPERTY_IMPL2(FontWeight, w, W, eight, WeightInherited)
	QF_PROPERTY_IMPL2(qreal, p, P, ointSize, 0)
	QF_PROPERTY_IMPL2(StrikeOut, s, S, trikeOut, StrikeOutInherited)
public:
	QFont font();
private:
	QFont m_font;
};

}}}}

#endif // QF_GUI_REPORTS_STYLE_FONT_H
