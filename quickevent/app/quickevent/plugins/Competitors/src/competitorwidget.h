#ifndef COMPETITORWIDGET_H
#define COMPETITORWIDGET_H

#include <qf/qmlwidgets/framework/datadialogwidget.h>

#include <plugins/Runs/src/runstablemodel.h>

namespace quickevent { namespace core {  namespace og { class SqlTableModel; }}}

namespace Ui {
class CompetitorWidget;
}

class CompetitorWidget : public qf::qmlwidgets::framework::DataDialogWidget
{
	Q_OBJECT
private:
	typedef qf::qmlwidgets::framework::DataDialogWidget Super;
public:
	explicit CompetitorWidget(QWidget *parent = nullptr);
	~CompetitorWidget() Q_DECL_OVERRIDE;

	bool load(const QVariant &id = QVariant(), int mode = qf::core::model::DataDocument::ModeEdit) Q_DECL_OVERRIDE;
	void loadFromRegistrations(int siid);
	//Q_SIGNAL void editStartListRequest(int stage_id, int class_id, int competitor_id);
	void save();
private slots:
	void onRegistrationSelected(const QVariantMap &values);
	void onSwitchNames();
private:
	Q_SLOT bool loadRunsTable();
	Q_SLOT bool saveRunsTable();
	//void onRunsTableCustomContextMenuRequest(const QPoint &pos);
	bool saveData() Q_DECL_OVERRIDE;

	QString guessClassFromRegistration(const QString &registration);

	// void showRunsTable(int stage_id);
private:
	Ui::CompetitorWidget *ui;
	quickevent::core::og::SqlTableModel *m_runsModel;
};

#endif // COMPETITORWIDGET_H
