#pragma once

#include <qf/qmlwidgets/framework/dialogwidget.h>

#include <plugins/Core/src/widgets/settingspage.h>

namespace Ui {
class LentCardsSettingsPage;
}

namespace qf {
namespace core { namespace model { class SqlTableModel; } }
}

class LentCardsSettingsPage : public Core::SettingsPage
{
	Q_OBJECT
private:
	using Super = Core::SettingsPage;

public:
	explicit LentCardsSettingsPage(QWidget *parent = 0);
	~LentCardsSettingsPage() override;
protected:
	void load() override;
	void save() override;

private:
	Ui::LentCardsSettingsPage *ui;
	qf::core::model::SqlTableModel *m_tableModel;
};

