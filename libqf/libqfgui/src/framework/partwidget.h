#ifndef QF_GUI_FRAMEWORK_PARTWIDGET_H
#define QF_GUI_FRAMEWORK_PARTWIDGET_H

#include "../guiglobal.h"
#include "ipersistentsettings.h"
#include "../toolbar.h"

#include <qf/core/utils.h>

#include <QWidget>

class QHBoxLayout;

namespace qf {
namespace gui {
class Frame;
class MenuBar;
namespace dialogs {
namespace internal {
class CaptionFrame;
}
}
namespace framework {

class Plugin;

class QFGUI_DECL_EXPORT PartWidget : public QWidget, public IPersistentSettings
{
	Q_OBJECT
	Q_PROPERTY(QString title READ title WRITE setTitle NOTIFY titleChanged)
	/// url or path to part icon
	Q_PROPERTY(QString iconSource READ iconSource WRITE setIconSource NOTIFY iconSourceChanged)
	Q_PROPERTY(QString featureId READ featureId WRITE setFeatureId NOTIFY featureIdChanged FINAL)
	Q_PROPERTY(QString persistentSettingsId READ persistentSettingsId WRITE setPersistentSettingsId)
	Q_PROPERTY(bool active READ isActive WRITE setActive NOTIFY activeChanged)
	Q_PROPERTY(qf::gui::MenuBar* menuBar READ menuBar)
private:
	typedef QWidget Super;
public:
	explicit PartWidget(QWidget *parent = nullptr) : PartWidget(QString(), parent) {}
	explicit PartWidget(const QString &feature_id, QWidget *parent = nullptr);
	~PartWidget() Q_DECL_OVERRIDE;

	QF_PROPERTY_BOOL_IMPL(a, A, ctive)
public:
	QString title() const { return m_title; }
	void setTitle(QString arg);
	Q_SIGNAL void titleChanged(const QString &s);

	QString iconSource() const { return m_iconSource; }
	void setIconSource(QString arg);
	Q_SIGNAL void iconSourceChanged(const QString &s);

	QString featureId() const { return m_featureId; }
	void setFeatureId(QString id);
	Q_SIGNAL void featureIdChanged(const QString &s);

	Q_SLOT void loadPersistentSettings();

	QIcon createIcon();
	Q_SLOT void updateCaptionFrame();

	MenuBar* menuBar();
	Q_INVOKABLE qf::gui::ToolBar* toolBar(const QString &name, bool create_if_not_exists = false);
	void addWidget(QWidget *w);
protected:
	Frame* centralFrame();
private:
	Q_SLOT void savePersistentSettings();
private:
	MenuBar *m_menuBar = nullptr;
	QMap<QString, ToolBar*> m_toolBars;
	QHBoxLayout *m_toolBarsLayout = nullptr;
	qf::gui::dialogs::internal::CaptionFrame *m_captionFrame;
	Frame *m_centralFrame;
	QString m_title;
	QString m_iconSource;
	QString m_featureId;
};

}}}

#endif // QF_GUI_FRAMEWORK_PARTWIDGET_H
