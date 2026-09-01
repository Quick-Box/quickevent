#pragma once
#include "awarddesign.h"

#include <QDialog>
#include <QShowEvent>
#include <QWheelEvent>

class AwardDesignerScene;
class AwardSceneItem;

namespace Ui {
class AwardDesignerDialog;
}

class AwardDesignerDialog : public QDialog
{
	Q_OBJECT
public:
	explicit AwardDesignerDialog(const QList<AwardDesigner::FieldDef> &available_fields,
		const AwardDesigner::Design &default_design,
		QWidget *parent = nullptr);
	~AwardDesignerDialog() override;

	void loadDesign(const AwardDesigner::Design &design);
	AwardDesigner::Design currentDesign() const;
	QString designName() const;

private:
	void onSelectedItemChanged(AwardSceneItem *item);
	void onAddFieldClicked();
	void onAddImageClicked();
	void onDeleteItemClicked();
	void onBrowseImageClicked();
	void onChooseColorClicked();
	void onItemPropertyChanged();
	void onSaveDesignClicked();
	void onLoadDesignClicked();
	void onNewDesignClicked();
	void accept() override;

protected:
	void showEvent(QShowEvent *event) override;
	bool eventFilter(QObject *obj, QEvent *event) override;

private:
	Ui::AwardDesignerDialog *ui;
	AwardDesignerScene *m_scene;
	QList<AwardDesigner::FieldDef> m_availableFields;
	QString m_designType;
	AwardSceneItem *m_selectedItem = nullptr;
	bool m_updatingProps = false;
	QString m_colorHex = QStringLiteral("#000000");

	void populatePropsFromItem(AwardSceneItem *item);
	void applyPropsToItem();
	void updateColorButton();
	void setPropsEnabled(bool enabled);
	void updateItemKindVisibility(AwardDesigner::Item::Kind kind);
};
