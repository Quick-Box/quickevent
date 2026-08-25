#include "font.h"

using namespace qf::gui::reports::style;

Font::Font(QObject *parent) :
	StyleObject(parent, StyleObject::SGFont)
{
}

Font::~Font() = default;

QFont Font::font()
{
	if(isDirty()) {
		setDirty(false);
		bool is_inherited = false;
		{
			QVariant v = basedOn();
			if(v.isValid()) {
				QObject *o = styleobjectFromVariant(v);
				Font *based_on = qobject_cast<Font*>(o);
				if(based_on) {
					is_inherited = true;
					m_font = based_on->font();
				}
			}
		}
		{
			FontStyle bs = style();
			if(bs == StyleInherited) {
				if(!is_inherited)
					m_font.setStyle((QFont::Style)StyleNormal);
			}
			else {
				m_font.setStyle((QFont::Style)bs);
			}
		}
		{
			FontStyleHint bs = hint();
			if(bs == HintInherited) {
				if(!is_inherited)
					m_font.setStyleHint((QFont::StyleHint)HintAnyStyle);
			}
			else {
				m_font.setStyleHint((QFont::StyleHint)bs);
			}
		}
		{
			FontWeight bs = weight();
			if(bs == WeightInherited) {
				if(!is_inherited)
					m_font.setWeight((QFont::Weight)WeightNormal);
			}
			else {
				m_font.setWeight((QFont::Weight)bs);
			}
		}
		{
			QString fam = family();
			if(!fam.isEmpty())
				m_font.setFamily(fam);
		}
		{
			qreal sz = pointSize();
			if(sz > 0)
				m_font.setPointSizeF(sz);
		}
		{
			StrikeOut so = strikeOut();
			if(so == StrikeOutInherited) {
				if(!is_inherited)
					m_font.setStrikeOut(false);
			} else {
				m_font.setStrikeOut(so == StrikeOutTrue);
			}
		}
	}
	return m_font;
}
