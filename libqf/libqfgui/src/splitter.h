#ifndef QF_GUI_SPLITTER_H
#define QF_GUI_SPLITTER_H

#include "guiglobal.h"
#include "framework/ipersistentsettings.h"

#include <QSplitter>

class QWidget;

namespace qf {
namespace gui {

class QFGUI_DECL_EXPORT Splitter : public QSplitter, public framework::IPersistentSettings
{
	Q_OBJECT
	Q_PROPERTY(QString persistentSettingsId READ persistentSettingsId WRITE setPersistentSettingsId)
public:
	explicit Splitter(QWidget *parent = nullptr);
	~Splitter() Q_DECL_OVERRIDE;
private:
	Q_SLOT void loadPersistentSettings();
	Q_SLOT void savePersistentSettings();
};

}}

#endif
