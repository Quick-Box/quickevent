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
	explicit AwardDesignerDialog(
		const QList<AwardDesigner::FieldDef> &available_fields,
		const AwardDesigner::Design &default_design,
		const QString &design_relative_root,
		QWidget *parent = nullptr);
	~AwardDesignerDialog() override;

	void loadDesign(const QString &file_name);
	void setDesign(const AwardDesigner::Design &design);
	AwardDesigner::Design currentDesign() const;
	QString designName() const;

	void accept() override;

protected:
	void showEvent(QShowEvent *event) override;
	bool eventFilter(QObject *obj, QEvent *event) override;

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

	void populatePropsFromItem(AwardSceneItem *item);
	void applyPropsToItem();
	void updateColorButton();
	void setPropsEnabled(bool enabled);
	void updateItemKindVisibility(AwardDesigner::Item::Kind kind);

	QString relativeDesignFilePath(const QString &file_name) const;
private:
	Ui::AwardDesignerDialog *ui;
	AwardDesignerScene *m_scene;
	QString m_relativeDesignFilesRoot;

	QList<AwardDesigner::FieldDef> m_availableFields;
	AwardSceneItem *m_selectedItem = nullptr;
	bool m_updatingProps = false;
	QString m_colorHex = QStringLiteral("#000000");
};
