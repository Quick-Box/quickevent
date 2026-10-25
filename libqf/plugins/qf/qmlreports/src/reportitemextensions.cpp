#include "reportitemextensions.h"

#include <qf/gui/reports/processor/reportitemframe.h>
#include <qf/gui/reports/processor/reportitemimage.h>
#include <qf/gui/reports/processor/reportitempara.h>
#include <qf/gui/reports/processor/style/sheet.h>

#include <qf/core/log.h>

#include <QDateTime>

using namespace qf::gui::reports;

namespace {

QQmlListProperty<ReportItem> frameItems(QObject *frame_object)
{
	using Index = qsizetype;
	auto *frame = qobject_cast<ReportItemFrame*>(frame_object);
	Q_ASSERT(frame);
	return QQmlListProperty<ReportItem>(frame, nullptr,
		[](QQmlListProperty<ReportItem> *lp, ReportItem *item) {
			static_cast<ReportItemFrame*>(lp->object)->addItem(item);
		},
		[](QQmlListProperty<ReportItem> *lp) -> Index {
			return static_cast<ReportItemFrame*>(lp->object)->itemCount();
		},
		[](QQmlListProperty<ReportItem> *lp, Index ix) {
			return static_cast<ReportItemFrame*>(lp->object)->itemAt(static_cast<int>(ix));
		},
		[](QQmlListProperty<ReportItem> *lp) {
			static_cast<ReportItemFrame*>(lp->object)->removeAllItems();
		});
}

style::Sheet* sheetObject(QObject *ext)
{
	auto *sheet = qobject_cast<style::Sheet*>(ext->parent());
	Q_ASSERT(sheet);
	return sheet;
}

}

//==========================================================
// FrameQmlExtension
//==========================================================
FrameQmlExtension::FrameQmlExtension(QObject *frame)
	: QObject(frame)
{
}

QQmlListProperty<ReportItem> FrameQmlExtension::items()
{
	return frameItems(parent());
}

//==========================================================
// ParaQmlExtension
//==========================================================
ParaQmlExtension::ParaQmlExtension(QObject *para)
	: QObject(para)
{
}

QQmlListProperty<ReportItem> ParaQmlExtension::items()
{
	return frameItems(parent());
}

void ParaQmlExtension::setTextFn(const QJSValue &val)
{
	if(!val.isCallable()) {
		qfError() << "JavaScript callable value (aka function) must be set to textFn property.";
		return;
	}
	m_textFn = val;
	auto *para = qobject_cast<ReportItemPara*>(parent());
	Q_ASSERT(para);
	para->setGetTextFunction([this]() {
		QJSValue jsv = m_textFn.call();
		if(jsv.isDate())
			return jsv.toDateTime().toString(Qt::ISODate);
		return jsv.toString();
	});
}

//==========================================================
// ImageQmlExtension
//==========================================================
ImageQmlExtension::ImageQmlExtension(QObject *image)
	: QObject(image)
{
}

QQmlListProperty<ReportItem> ImageQmlExtension::items()
{
	return frameItems(parent());
}

void ImageQmlExtension::setDataFn(const QJSValue &val)
{
	if(!val.isCallable()) {
		qfError() << "JavaScript callable value (aka function) must be set to dataFn property.";
		return;
	}
	m_dataFn = val;
	auto *image = qobject_cast<ReportItemImage*>(parent());
	Q_ASSERT(image);
	image->setGetDataFunction([this]() {
		return m_dataFn.call().toString();
	});
}

//==========================================================
// SheetQmlExtension
//==========================================================
SheetQmlExtension::SheetQmlExtension(QObject *sheet)
	: QObject(sheet)
{
}

QQmlListProperty<style::Color> SheetQmlExtension::colors()
{
	auto *sheet = sheetObject(this);
	return QQmlListProperty<style::Color>(sheet, &sheet->colors());
}

QQmlListProperty<style::Pen> SheetQmlExtension::pens()
{
	auto *sheet = sheetObject(this);
	return QQmlListProperty<style::Pen>(sheet, &sheet->pens());
}

QQmlListProperty<style::Brush> SheetQmlExtension::brushes()
{
	auto *sheet = sheetObject(this);
	return QQmlListProperty<style::Brush>(sheet, &sheet->brushes());
}

QQmlListProperty<style::Font> SheetQmlExtension::fonts()
{
	auto *sheet = sheetObject(this);
	return QQmlListProperty<style::Font>(sheet, &sheet->fonts());
}

QQmlListProperty<style::Text> SheetQmlExtension::textStyles()
{
	auto *sheet = sheetObject(this);
	return QQmlListProperty<style::Text>(sheet, &sheet->textStyles());
}
