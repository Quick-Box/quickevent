#include "reportitempara.h"

#include "reportprocessor.h"
#include "reportpainter.h"

#include <qf/core/log.h>
#include <qf/core/assert.h>
#include <qf/core/string.h>

#include <QDateTime>
#include <QRegularExpression>

using namespace qf::gui::reports;

//==========================================================
//                                    ReportItemPara
//==========================================================
ReportItemPara::ReportItemPara(ReportItem * parent)
	: ReportItemFrame(parent)
{
	qfLogFuncFrame();
	//qfInfo() << el.text();
}

void ReportItemPara::resetIndexToPrintRecursively(bool including_para_texts)
{
	if(including_para_texts)
		m_indexToPrint = 0;
}

ReportItem::PrintResult ReportItemPara::printMetaPaint(QPaintDevice *paint_device, ReportItemMetaPaint *out, const Rect &bounding_rect)
{
	qfLogFuncFrame();
	return Super::printMetaPaint(paint_device, out, bounding_rect);
}

ReportItem::PrintResult ReportItemPara::printMetaPaintChildren(QPaintDevice *paint_device, ReportItemMetaPaint *out, const ReportItem::Rect &bounding_rect)
{
	qfLogFuncFrame() << this << bounding_rect.toString() << paraText();;
	PrintResult res = PrintResult::createPrintFinished();
	if(m_indexToPrint == 0) {
		printedText = paraText();
	}
	QString text = printedText.mid(m_indexToPrint);
	//qfWarning() << printedText;
	int initial_index_to_print = m_indexToPrint;

	QString sql_id = sqlId();
	/// tiskne se prazdny text
	bool omit_empty_text = isOmitEmptyText();
	if(text.isEmpty() && omit_empty_text) {
	}
	else {
		QString text_to_layout = text;
		//qfWarning() << "length: " << text.length() << " text: [" << text << "]\n" << text.toUtf8().toHex();
		bool text_item_should_be_created = true;
		style::CompiledTextStyle style;
		style::Text *p_text_style = effectiveTextStyle();
		if(p_text_style) {
			style = p_text_style->textStyle();
		}
		auto font_metrics = QFontMetricsF(style.font(), paint_device);
		QTextOption text_option;
		{
			if(isTextWrap())
				text_option.setWrapMode(QTextOption::WrapAtWordBoundaryOrAnywhere);
			//alignment_flags |= Qt::TextWordWrap;
			int al = static_cast<int>(textHAlign()) | static_cast<int>(textVAlign());
			auto alignment_flags = (Qt::Alignment)al;
			text_option.setAlignment(alignment_flags);
		}
		Rect device_bounding_rect;
		/// velikost boundingRect je v mm, tak to prepocitej na body vystupniho zarizeni
		device_bounding_rect = gui::graphics::mm2device(bounding_rect, paint_device);

		bool render_check_mark = false;
		auto rx = ReportItemMetaPaint::checkReportSubstitutionRegExp;
		if(auto match = rx.match(text_to_layout); match.hasMatch()) {
			//bool check_on = rx.capturedTexts().value(1) == "1";
			device_bounding_rect = font_metrics.boundingRect('X');
			render_check_mark = true;
			m_indexToPrint += text_to_layout.length();
		}
		else {
			if(text_to_layout.isEmpty()) {
				/// neni omitEmptyString, takze i prazdnej text vyrendruj alespon jako mezeru aby se na to dalo treba kliknout
				text_to_layout = ' ';
			}
			/// do layout
			{
				qreal leading = font_metrics.leading();
				qreal height = 0;
				qreal width = 0;
				textLayout.setFont(style.font());
				textLayout.setTextOption(text_option);
				textLayout.setText(text_to_layout);
				textLayout.beginLayout();
				bool finished = false;
				while (!finished) {
					QTextLine line = textLayout.createLine();
					finished = !line.isValid();
					if(!finished) {
						line.setLineWidth(device_bounding_rect.width()); /// setWidth() nastavi spravne line.height(), proto musi byt pred merenim popsane vysky.

						if((line.textLength() == 0) && (line.textStart() + line.textLength() == text_to_layout.length())) {
							/// nevim kde je chyba, pri vicerakovych textech mi to pridava jeden prazdnej radek na konec, takhle se tomu snazim zabranit (Qt 4.6.3)
							finished = true;
						}
						else {
							qreal interline_space = (height > 0)? leading: 0;
							if(height + interline_space + line.height() > device_bounding_rect.height()) {
								res = PrintResult::createPrintAgain();
								if(height == 0) {
									/// nevejde se ani jeden radek
									text_item_should_be_created = false;
									break;
								}
																	/// neco se preci jenom veslo
									int pos = line.textStart();
									m_indexToPrint += pos;
									break;

							}
							height += interline_space;
							line.setPosition(QPointF(0., height));
							height += line.height();
							width = qMax(width, line.naturalTextWidth());
						}
					}
					if(finished) {
						m_indexToPrint = printedText.length();
					}
				}
				textLayout.endLayout();
				device_bounding_rect.setWidth(width);
				device_bounding_rect.setHeight(height);
			}
		}
		/// velikost boundingRect je v bodech vystupniho zarizeni, tak to prepocitej na mm
		device_bounding_rect = gui::graphics::device2mm(device_bounding_rect, paint_device);
		/// rendered rect is left aligned, if text is reight aligned or centered, the ReportItemMetaPaintText::paint() does it
		if(text_item_should_be_created ) {
			ReportItemMetaPaintText *mt;
			if(render_check_mark )
				mt = new ReportItemMetaPaintCheck(out, this);
			else {
				mt = new ReportItemMetaPaintText(out, this);
				mt->sqlId = sql_id;
				//--mt->editGrants = elementAttribute("editGrants");
			}
			//qfInfo() << "creating item:" << mt;
			mt->pen = style.pen();
			mt->font = style.font();
			mt->text = text.mid(0, m_indexToPrint - initial_index_to_print);
			//qfWarning() << "text:" << text;
			mt->textOption = text_option;
			mt->renderedRect = device_bounding_rect;
			mt->renderedRect.flags = designedRect.flags;
		}
		//qfDebug().color(QFLog::Green, QFLog::Red) << "\tleading:" << processor()->fontMetrics(style.font).leading() << "\theight:" << processor()->fontMetrics(style.font).height();
		qfDebug() << "\tchild rendered rect:" << device_bounding_rect.toString();
	}
	qfDebug() << "\t<<< CHILDREN paraText return:" << res.toString();
	return res;
}

void ReportItemPara::setTextFn(const QJSValue &val)
{
	if(val.isCallable()) {
		m_getTextJsFn = val;
	}
	else {
		qfError() << "JavaScript callable value (aka function) must be set to textFn property.";
	}
}

QString ReportItemPara::paraText()
{
	//qfLogFuncFrame();
	QString ret;
	if(m_getTextJsFn.isCallable()) {
		QJSValue jsv = m_getTextJsFn.call();
		if(jsv.isDate()) {
			QDateTime dt = jsv.toDateTime();
			ret = dt.toString(Qt::ISODate);//.date().toString(Qt::ISODate);
		}
		else
			ret = jsv.toString();
	}
	else if(m_getTextCppFn) {
		ret = m_getTextCppFn();
	}
	else {
		ret = text();
	}
	{
		static QString new_line;
		if(new_line.isEmpty())
			new_line += QChar::LineSeparator;
		ret.replace("\\n", new_line);
		ret.replace("\n", new_line);

		/// jinak nedokazu zadat mezeru mezi dvema <data> elementy nez <data>\s<data>
		ret.replace("\\s", " ");
		/// non breaking space
		ret.replace("\\S", QString(QChar::Nbsp));
	}
	return ret;
}

