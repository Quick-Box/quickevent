#ifndef QF_GUI_REPORTS_STYLE_STYLE_OBJECT_H
#define QF_GUI_REPORTS_STYLE_STYLE_OBJECT_H

#include "../../../guiglobal.h"

#include <QObject>

class QString;
class QVariant;
class QEvent;

namespace qf {
namespace gui {
namespace reports {
class ReportItemReport;

class ReportProcessor;

namespace style {

class Sheet;

class QFGUI_DECL_EXPORT StyleObject : public QObject
{
	Q_OBJECT
	Q_PROPERTY(QString name READ name WRITE setName NOTIFY nameChanged)
private:
	typedef QObject Super;
public:
    enum StyleGroup {SGOwn, SGColor, SGPen, SGBrush, SGFont, SGText};
public:
    StyleObject(QObject *parent = nullptr, StyleGroup style_group = SGOwn);
public:
	StyleGroup styleGroup() {return m_styleGroup;}
    QString name();
    virtual void setName(const QString &s);
	Q_SIGNAL void nameChanged(const QString &new_name);
protected:
	ReportItemReport* reportItem();
	ReportProcessor* reportProcessor();
    Sheet* rootStyleSheet();

    QObject* styleobjectFromVariant(const QVariant &v, StyleGroup style_group = SGOwn, bool should_exist = true);

    //QString nextSequentialName();

    Q_SLOT void setDirty(bool b = true) {m_dirty = b;}
    bool isDirty() {return m_dirty;}
private:
	//void updateStyleCache(const QString &old_name, const QString &new_name);
private:
    StyleGroup m_styleGroup;
    bool m_dirty;
};

}}}}

#endif
