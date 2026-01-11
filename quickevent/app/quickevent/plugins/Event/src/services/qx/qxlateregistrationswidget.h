#pragma once

#include <QWidget>

namespace qf::gui::model { class SqlTableModel; }

namespace Event::services::qx {

namespace Ui {
class QxLateRegistrationsWidget;
}

class QxEventService;

class QxLateRegistrationsWidget : public QWidget
{
	Q_OBJECT

public:
	explicit QxLateRegistrationsWidget(QWidget *parent = nullptr);
	~QxLateRegistrationsWidget() override;

	void onDbEventNotify(const QString &domain, int connection_id, const QVariant &payload);
	void onVisibleChanged(bool is_visible);
private:
	QxEventService* service();
	void reload();
	void addQxChangeRow(int sql_id);

	void resizeColumns();
	void showMessage(const QString &msg, bool is_error = false);
	void applyCurrentChange();

	void onTableCustomContextMenuRequest(const QPoint &pos);
	void onTableDoubleClicked(const QModelIndex &ix);

private:
	Ui::QxLateRegistrationsWidget *ui;
	qf::gui::model::SqlTableModel *m_model;
};

}

