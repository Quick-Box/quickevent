#ifndef STACKEDCENTRALWIDGET_H
#define STACKEDCENTRALWIDGET_H

#include "../guiglobal.h"
#include "centralwidget.h"

class QStackedWidget;

namespace qf {
namespace gui {
namespace framework {

class PartWidget;
class PartSwitch;

class QFGUI_DECL_EXPORT StackedCentralWidget : public CentralWidget
{
	Q_OBJECT
private:
	typedef CentralWidget Super;
public:
	explicit StackedCentralWidget(MainWindow *parent = nullptr);
	~StackedCentralWidget() Q_DECL_OVERRIDE;
public:
	void addPartWidget(PartWidget *widget) Q_DECL_OVERRIDE;
	PartWidget* partWidget(int part_index);

	PartSwitch* partSwitch() {return m_partSwitch;}

	using Super::setActivePart;
	bool setActivePart(int part_index, bool set_active)  Q_DECL_OVERRIDE;
	int featureToIndex(const QString &feature_id) Q_DECL_OVERRIDE;
	void setPartVisible(const QString &feature_id, bool visible);
private:
	QStackedWidget *m_centralWidget;
	PartSwitch *m_partSwitch;
};

}}}

#endif // STACKEDCENTRALWIDGET_H
