#ifndef QF_QMLWIDGETS_DIALOGS_QMLDIALOG_H
#define QF_QMLWIDGETS_DIALOGS_QMLDIALOG_H

#include "../qmlwidgetsglobal.h"
#include "dialog.h"
#include "../framework/ipersistentsettings.h"
#include "../framework/dialogwidget.h"

#include <QQmlListProperty>
#include <QQmlParserStatus>

namespace qf {
namespace qmlwidgets {

class Frame;

namespace dialogs {

class QFQMLWIDGETS_DECL_EXPORT QmlDialog : public Dialog, public QQmlParserStatus
{
	Q_OBJECT
	Q_INTERFACES(QQmlParserStatus)
	Q_ENUMS(DoneResult)
	Q_ENUMS(QDialogButtonBox::StandardButton)
	Q_CLASSINFO("DefaultProperty", "widgets")
	Q_PROPERTY(QQmlListProperty<QWidget> widgets READ widgets)
	Q_PROPERTY(qf::qmlwidgets::framework::DialogWidget* dialogWidget READ dialogWidget WRITE setDialogWidget NOTIFY dialogWidgetChanged)
	Q_PROPERTY(int standardButtons READ standardButtons WRITE setStandardButtons)
private:
	typedef Dialog Super;
public:
	explicit QmlDialog(QWidget *parent = nullptr);
	~QmlDialog() Q_DECL_OVERRIDE;

	void setStandardButtons(int bts) {setButtons((QDialogButtonBox::StandardButtons)bts);}
	int standardButtons();

	qf::qmlwidgets::framework::DialogWidget* dialogWidget() const;
	Q_SLOT void setDialogWidget(qf::qmlwidgets::framework::DialogWidget* w);
	Q_SIGNAL void dialogWidgetChanged(framework::DialogWidget* w);
protected:
	QQmlListProperty<QWidget> widgets();
private:
	void classBegin() Q_DECL_OVERRIDE;
	void componentComplete() Q_DECL_OVERRIDE;
protected:
	Frame *m_centralFrame;
};

}}}

#endif // QF_QMLWIDGETS_DIALOGS_QMLDIALOG_H
