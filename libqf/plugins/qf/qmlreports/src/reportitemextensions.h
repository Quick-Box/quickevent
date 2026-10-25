#pragma once

#include <QJSValue>
#include <QObject>
#include <QQmlListProperty>

namespace qf::gui::reports {
class ReportItem;
namespace style {
class Color;
class Pen;
class Brush;
class Font;
class Text;
}
}

// QML extension objects adding QML specific properties to report items,
// so report processor classes do not depend on QML

class FrameQmlExtension : public QObject
{
	Q_OBJECT
	Q_CLASSINFO("DefaultProperty", "items")
	Q_PROPERTY(QQmlListProperty<qf::gui::reports::ReportItem> items READ items)
public:
	explicit FrameQmlExtension(QObject *frame);
	QQmlListProperty<qf::gui::reports::ReportItem> items();
};

class ParaQmlExtension : public QObject
{
	Q_OBJECT
	Q_CLASSINFO("DefaultProperty", "items")
	Q_PROPERTY(QQmlListProperty<qf::gui::reports::ReportItem> items READ items)
	Q_PROPERTY(QJSValue textFn READ textFn WRITE setTextFn)
public:
	explicit ParaQmlExtension(QObject *para);
	QQmlListProperty<qf::gui::reports::ReportItem> items();

	QJSValue textFn() const {return m_textFn;}
	void setTextFn(const QJSValue &val);
private:
	QJSValue m_textFn;
};

class ImageQmlExtension : public QObject
{
	Q_OBJECT
	Q_CLASSINFO("DefaultProperty", "items")
	Q_PROPERTY(QQmlListProperty<qf::gui::reports::ReportItem> items READ items)
	Q_PROPERTY(QJSValue dataFn READ dataFn WRITE setDataFn)
public:
	explicit ImageQmlExtension(QObject *image);
	QQmlListProperty<qf::gui::reports::ReportItem> items();

	QJSValue dataFn() const {return m_dataFn;}
	void setDataFn(const QJSValue &val);
private:
	QJSValue m_dataFn;
};

class SheetQmlExtension : public QObject
{
	Q_OBJECT
	Q_PROPERTY(QQmlListProperty<qf::gui::reports::style::Color> colors READ colors)
	Q_PROPERTY(QQmlListProperty<qf::gui::reports::style::Pen> pens READ pens)
	Q_PROPERTY(QQmlListProperty<qf::gui::reports::style::Brush> brushes READ brushes)
	Q_PROPERTY(QQmlListProperty<qf::gui::reports::style::Font> fonts READ fonts)
	Q_PROPERTY(QQmlListProperty<qf::gui::reports::style::Text> textStyles READ textStyles)
public:
	explicit SheetQmlExtension(QObject *sheet);

	QQmlListProperty<qf::gui::reports::style::Color> colors();
	QQmlListProperty<qf::gui::reports::style::Pen> pens();
	QQmlListProperty<qf::gui::reports::style::Brush> brushes();
	QQmlListProperty<qf::gui::reports::style::Font> fonts();
	QQmlListProperty<qf::gui::reports::style::Text> textStyles();
};
