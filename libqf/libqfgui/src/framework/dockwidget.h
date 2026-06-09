#ifndef QF_GUI_FRAMEWORK_DOCKWIDGET_H
#define QF_GUI_FRAMEWORK_DOCKWIDGET_H

#include "../guiglobal.h"
#include "../framework/ipersistentsettings.h"

#include <QDockWidget>

namespace qf {
namespace gui {
class Frame;
namespace framework {

class QFGUI_DECL_EXPORT DockWidget : public QDockWidget, public framework::IPersistentSettings
{
	Q_OBJECT
	Q_PROPERTY(QWidget* widget READ widget WRITE setQmlWidget)
	Q_CLASSINFO("DefaultProperty", "widget")
	Q_PROPERTY(QString persistentSettingsId READ persistentSettingsId WRITE setPersistentSettingsId)
private:
	typedef QDockWidget Super;
public:
	explicit DockWidget(const QString &window_title, QWidget *parent = nullptr, Qt::WindowFlags flags = Qt::WindowFlags());
	explicit DockWidget(QWidget *parent = nullptr, Qt::WindowFlags flags = Qt::WindowFlags()) : DockWidget(QString(), parent, flags) {}
	~DockWidget() override;

	// visibilityChanged() exists already in QDockWidget
	//Q_SIGNAL void visibleChanged(bool visible);
protected:
	bool event(QEvent *ev) override;
	//void showEvent(QShowEvent *ev) Q_DECL_OVERRIDE;
private:
	void setQmlWidget(QWidget *w);
};

}}}

#endif // QF_GUI_FRAMEWORK_DOCKWIDGET_H
