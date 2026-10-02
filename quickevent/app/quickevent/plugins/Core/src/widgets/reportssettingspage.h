#ifndef REPORTSSETTINGSPAGE_H
#define REPORTSSETTINGSPAGE_H

#include "settingspage.h"

class QSortFilterProxyModel;

namespace Core {

namespace Ui {
class ReportsSettingsPage;
}

class ReportsTableModel;

class ReportsSettingsPage : public Core::SettingsPage
{
	Q_OBJECT

	using Super = Core::SettingsPage;
public:
	explicit ReportsSettingsPage(QWidget *parent = nullptr);
	~ReportsSettingsPage() override;
private:
	void load() override;
	void save() override;

	void resizeTableColumnsToFit();
	void showReportContextMenu(const QPoint &position);
	void loadModel();

private:
	Ui::ReportsSettingsPage *ui;
	ReportsTableModel *m_reportModel;
	QSortFilterProxyModel *m_reportProxyModel;
};

}
#endif // REPORTSSETTINGSPAGE_H
