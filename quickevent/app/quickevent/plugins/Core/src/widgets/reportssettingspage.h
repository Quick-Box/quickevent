#ifndef REPORTSSETTINGSPAGE_H
#define REPORTSSETTINGSPAGE_H

#include "settingspage.h"

namespace Core {

namespace Ui {
class ReportsSettingsPage;
}

class ReportsSettingsPage : public Core::SettingsPage
{
	Q_OBJECT

	using Super = Core::SettingsPage;
public:
	explicit ReportsSettingsPage(QWidget *parent = nullptr);
	~ReportsSettingsPage();

	QString reportsDirectoryFromSettings() const;
private slots:
	void onSelectCustomReportsDirectoryClicked();
private:
	void load() override;
	void save() override;

	void setReportsDirectory(const QString dir);
private:
	Ui::ReportsSettingsPage *ui;
	//QString m_exportReportDefinitionsDir;
};

}
#endif // REPORTSSETTINGSPAGE_H
