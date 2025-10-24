#pragma once

#include <qf/gui/framework/datadialogwidget.h>

#include <plugins/Runs/src/runstablemodel.h>

namespace quickevent { namespace core {  namespace og { class SqlTableModel; }}}

namespace Ui {
class CompetitorWidget;
}

class CompetitorWidget : public qf::gui::framework::DataDialogWidget
{
	Q_OBJECT
private:
	typedef qf::gui::framework::DataDialogWidget Super;
public:
	explicit CompetitorWidget(QWidget *parent = nullptr);
	~CompetitorWidget() override;

	bool load(const QVariant &id = QVariant(), int mode = qf::gui::model::DataDocument::ModeEdit) override;
	void loadFromRegistrations(int siid);
	void save();
private:
	void onRegistrationSelected(const QVariantMap &values);
	void onSwitchNames();
	bool loadRunsTable();
	bool saveRunsTable();
	// void onRunsTableCustomContextMenuRequest(const QPoint &pos);
	bool saveData() override;

	bool acceptDialogDone(int result) override;

	QString guessClassFromRegistration(const QString &registration);

	static QList<int> possibleStartTimesMs(int run_id);
private:
	Ui::CompetitorWidget *ui;
	quickevent::gui::og::SqlTableModel *m_runsModel;
};

