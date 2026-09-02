#pragma once

#include <QFrame>

#include "partwidget.h"

class QCheckBox;
class RelaysTableItemDelegate;

namespace Ui {
class RelaysWidget;
}

namespace qf::gui::model { class SqlTableModel; }
namespace qf::gui { class ForeignKeyComboBox; }

class RelaysWidget : public QFrame
{
	Q_OBJECT
private:
	typedef QFrame Super;
public:
	explicit RelaysWidget(QWidget *parent = nullptr);
	~RelaysWidget() Q_DECL_OVERRIDE;

	void settleDownInPartWidget(::PartWidget *part_widget);
private:
	Q_SLOT void lazyInit();
	Q_SLOT void reset();
	Q_SLOT void reload();

	void editRelay(const QVariant &id, int mode);
	void editRelays(int mode);

	//void onDbEventNotify(const QString &domain, int connection_id, const QVariant &data);

	QVariant startListByClubsTableData(bool with_vacants);

	void relays_assignNumbers();
	void relays_importBibs();
	void relays_addVacants();

	void print_start_list_classes();
	void print_start_list_clubs();

	void print_results_nlegs();
	void print_results_overal();
	void print_results_overal_condensed();
	void print_results_awards();

	void export_results_iofxml3();
	void export_start_list_iofxml3();

	//void printResults(const QString &settings_id, const QVariantMap &default_options);
	void updateClassComboBox();
private:
	Ui::RelaysWidget *ui;
	qf::gui::model::SqlTableModel *m_tblModel;
	qf::gui::ForeignKeyComboBox *m_cbxClasses = nullptr;
	RelaysTableItemDelegate *m_relaysTableItemDelegate = nullptr;

	void save_xml_file(QString str, QString fn);
};

