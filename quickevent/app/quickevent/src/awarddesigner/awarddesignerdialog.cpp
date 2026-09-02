#include "awarddesignerdialog.h"
#include "ui_awarddesignerdialog.h"
#include "awarddesignerscene.h"

#include <QColorDialog>
#include <QFileDialog>
#include <QFontComboBox>
#include <QGraphicsView>
#include <QInputDialog>
#include <QMessageBox>
#include <QPainter>
#include <QTimer>
#include <QWheelEvent>

AwardDesignerDialog::AwardDesignerDialog(const QList<AwardDesigner::FieldDef> &available_fields,
	const AwardDesigner::Design &default_design,
	QWidget *parent)
	: QDialog(parent)
	, ui(new Ui::AwardDesignerDialog)
	, m_availableFields(available_fields)
	, m_designType(default_design.type)
{
	ui->setupUi(this);

	m_scene = new AwardDesignerScene(this);
	m_scene->setAvailableFields(m_availableFields);
	ui->graphicsView->setScene(m_scene);
	ui->graphicsView->setRenderHints(QPainter::Antialiasing | QPainter::SmoothPixmapTransform);
	ui->graphicsView->setDragMode(QGraphicsView::NoDrag);
	ui->graphicsView->setTransformationAnchor(QGraphicsView::AnchorUnderMouse);
	ui->graphicsView->setResizeAnchor(QGraphicsView::AnchorViewCenter);
	ui->graphicsView->setBackgroundBrush(QColor(130, 130, 130));
	ui->graphicsView->installEventFilter(this);

	// Populate field selector in properties panel
	for (const auto &fd : m_availableFields) {
		ui->cbxItemField->addItem(fd.label, fd.id);
	}

	ui->cbxAlign->clear();
	ui->cbxAlign->addItem(tr("Vlevo"), static_cast<int>(Qt::AlignLeft));
	ui->cbxAlign->addItem(tr("Na střed"), static_cast<int>(Qt::AlignHCenter));
	ui->cbxAlign->addItem(tr("Vpravo"), static_cast<int>(Qt::AlignRight));

	connect(m_scene, &AwardDesignerScene::selectedItemChanged,
		this, &AwardDesignerDialog::onSelectedItemChanged);

	connect(ui->btnAddField, &QPushButton::clicked, this, &AwardDesignerDialog::onAddFieldClicked);
	connect(ui->btnAddImage, &QPushButton::clicked, this, &AwardDesignerDialog::onAddImageClicked);
	connect(ui->btnDelete, &QPushButton::clicked, this, &AwardDesignerDialog::onDeleteItemClicked);
	connect(ui->btnBrowseImage, &QPushButton::clicked, this, &AwardDesignerDialog::onBrowseImageClicked);
	connect(ui->btnColor, &QPushButton::clicked, this, &AwardDesignerDialog::onChooseColorClicked);
	connect(ui->btnSave, &QPushButton::clicked, this, &AwardDesignerDialog::onSaveDesignClicked);
	connect(ui->btnLoad, &QPushButton::clicked, this, &AwardDesignerDialog::onLoadDesignClicked);
	connect(ui->btnNew, &QPushButton::clicked, this, &AwardDesignerDialog::onNewDesignClicked);

	auto prop_changed = [this]() { onItemPropertyChanged(); };
	connect(ui->spX, qOverload<double>(&QDoubleSpinBox::valueChanged), this, prop_changed);
	connect(ui->spY, qOverload<double>(&QDoubleSpinBox::valueChanged), this, prop_changed);
	connect(ui->spW, qOverload<double>(&QDoubleSpinBox::valueChanged), this, prop_changed);
	connect(ui->spH, qOverload<double>(&QDoubleSpinBox::valueChanged), this, prop_changed);
	connect(ui->cbxFont, &QFontComboBox::currentFontChanged, this, prop_changed);
	connect(ui->spFontSize, qOverload<int>(&QSpinBox::valueChanged), this, prop_changed);
	connect(ui->chkBold, &QCheckBox::toggled, this, prop_changed);
	connect(ui->chkItalic, &QCheckBox::toggled, this, prop_changed);
	connect(ui->cbxAlign, qOverload<int>(&QComboBox::currentIndexChanged), this, prop_changed);
	connect(ui->cbxItemField, qOverload<int>(&QComboBox::currentIndexChanged), this, prop_changed);
	connect(ui->edCustomText, &QLineEdit::editingFinished, this, prop_changed);
	connect(ui->chkScaleProportional, &QCheckBox::toggled, this, prop_changed);

	connect(ui->btnFitView, &QPushButton::clicked, this, [this]() {
		ui->graphicsView->fitInView(m_scene->sceneRect(), Qt::KeepAspectRatio);
	});
	connect(ui->btnZoomIn, &QPushButton::clicked, this, [this]() {
		ui->graphicsView->scale(1.25, 1.25);
	});
	connect(ui->btnZoomOut, &QPushButton::clicked, this, [this]() {
		ui->graphicsView->scale(1.0 / 1.25, 1.0 / 1.25);
	});

	setPropsEnabled(false);

	// Load default design on first open
	loadDesign(default_design);
}

AwardDesignerDialog::~AwardDesignerDialog()
{
	delete ui;
}

void AwardDesignerDialog::loadDesign(const AwardDesigner::Design &design)
{
	ui->edDesignName->setText(design.name);
	m_scene->loadDesign(design);
	QTimer::singleShot(0, this, [this]() {
		ui->graphicsView->fitInView(m_scene->sceneRect(), Qt::KeepAspectRatio);
	});
}

AwardDesigner::Design AwardDesignerDialog::currentDesign() const
{
	return m_scene->collectDesign(ui->edDesignName->text().trimmed());
}

QString AwardDesignerDialog::designName() const
{
	return ui->edDesignName->text().trimmed();
}

void AwardDesignerDialog::onSelectedItemChanged(AwardSceneItem *item)
{
	m_selectedItem = item;
	setPropsEnabled(item != nullptr);
	if (item) {
		populatePropsFromItem(item);
	}
}

void AwardDesignerDialog::onAddFieldClicked()
{
	AwardDesigner::Item item;
	item.kind = AwardDesigner::Item::Field;
	item.fieldId = QStringLiteral("eventName");
	item.w = 170;
	item.h = 15;
	int count = m_scene->collectDesign().items.count();
	item.x = 20 + (count % 8) * 3;
	item.y = 20 + (count % 8) * 3;
	m_scene->addDesignItem(item);
}

void AwardDesignerDialog::onAddImageClicked()
{
	QString path = QFileDialog::getOpenFileName(this,
		tr("Vyberte obrázek"),
		QString(),
		tr("Obrázky (*.png *.jpg *.jpeg *.svg *.bmp);;Všechny soubory (*)"));

	AwardDesigner::Item item;
	item.kind = AwardDesigner::Item::Image;
	item.imagePath = path; // may be empty — user can browse again in properties
	item.w = 80;
	item.h = 50;
	int count = m_scene->collectDesign().items.count();
	item.x = 20 + (count % 8) * 3;
	item.y = 20 + (count % 8) * 3;
	m_scene->addDesignItem(item);
}

void AwardDesignerDialog::onDeleteItemClicked()
{
	m_scene->deleteSelected();
}

void AwardDesignerDialog::onBrowseImageClicked()
{
	QString path = QFileDialog::getOpenFileName(this,
		tr("Vyberte obrázek"),
		QString(),
		tr("Obrázky (*.png *.jpg *.jpeg *.svg *.bmp);;Všechny soubory (*)"));
	if (!path.isEmpty()) {
		ui->edImagePath->setText(path);
		if (!m_updatingProps) {
			applyPropsToItem();
		}
	}
}

void AwardDesignerDialog::onChooseColorClicked()
{
	QColor c = QColorDialog::getColor(QColor(m_colorHex), this, tr("Zvolte barvu textu"));
	if (c.isValid()) {
		m_colorHex = c.name();
		updateColorButton();
		if (!m_updatingProps) {
			applyPropsToItem();
		}
	}
}

void AwardDesignerDialog::onItemPropertyChanged()
{
	if (!m_updatingProps) {
		applyPropsToItem();
	}
}

void AwardDesignerDialog::onSaveDesignClicked()
{
	QString name = ui->edDesignName->text().trimmed();
	if (name.isEmpty()) {
		QMessageBox::warning(this, tr("Uložit návrh"), tr("Zadejte prosím název návrhu."));
		ui->edDesignName->setFocus();
		return;
	}
	AwardDesigner::Design d = m_scene->collectDesign(name);
	d.type = m_designType;
	if (d.saveToDb()) {
		QMessageBox::information(this, tr("Uložit návrh"),
			tr("Návrh '%1' byl uložen do databáze.").arg(name));
	} else {
		QMessageBox::critical(this, tr("Uložit návrh"),
			tr("Návrh '%1' se nepodařilo uložit do databáze.").arg(name));
	}
}

void AwardDesignerDialog::onLoadDesignClicked()
{
	QStringList designs = AwardDesigner::Design::listFromDb(m_designType);
	if (designs.isEmpty()) {
		QMessageBox::information(this, tr("Načíst návrh"),
			tr("V databázi nejsou uloženy žádné návrhy diplomů."));
		return;
	}
	bool ok;
	QString name = QInputDialog::getItem(this,
		tr("Načíst návrh"), tr("Vyberte návrh:"), designs, 0, false, &ok);
	if (!ok || name.isEmpty()) {
		return;
	}
	AwardDesigner::Design d = AwardDesigner::Design::loadFromDb(name);
	if (d.isValid()) {
		loadDesign(d);
	}
}

void AwardDesignerDialog::onNewDesignClicked()
{
	AwardDesigner::Design empty;
	ui->edDesignName->clear();
	m_scene->loadDesign(empty);
}

void AwardDesignerDialog::accept()
{
	QString name = ui->edDesignName->text().trimmed();
	if (!name.isEmpty()) {
		AwardDesigner::Design d = m_scene->collectDesign(name);
		d.type = m_designType;
		if (!d.saveToDb()) {
			QMessageBox::critical(this, tr("Uložit návrh"),
				tr("Návrh '%1' se nepodařilo uložit do databáze.").arg(name));
			return;
		}
	}
	QDialog::accept();
}

void AwardDesignerDialog::populatePropsFromItem(AwardSceneItem *item)
{
	m_updatingProps = true;
	const AwardDesigner::Item &it = item->designItem();

	updateItemKindVisibility(it.kind);

	ui->spX->setValue(it.x);
	ui->spY->setValue(it.y);
	ui->spW->setValue(it.w);
	ui->spH->setValue(it.h);

	if (it.kind == AwardDesigner::Item::Field) {
		// Select field in combo
		for (int i = 0; i < ui->cbxItemField->count(); ++i) {
			if (ui->cbxItemField->itemData(i).toString() == it.fieldId) {
				ui->cbxItemField->setCurrentIndex(i);
				break;
			}
		}
		// Custom text (shown only when fieldId == "customText")
		ui->edCustomText->setText(it.customText);
		ui->lblCustomText->setVisible(it.fieldId == QLatin1String("customText"));
		ui->edCustomText->setVisible(it.fieldId == QLatin1String("customText"));

		ui->cbxFont->setCurrentFont(QFont(it.fontFamily));
		ui->spFontSize->setValue(it.fontSize);
		ui->chkBold->setChecked(it.bold);
		ui->chkItalic->setChecked(it.italic);
		m_colorHex = it.color;
		updateColorButton();
		for (int i = 0; i < ui->cbxAlign->count(); ++i) {
			if (ui->cbxAlign->itemData(i).toInt() == it.halign) {
				ui->cbxAlign->setCurrentIndex(i);
				break;
			}
		}
	} else {
		ui->edImagePath->setText(it.imagePath);
		ui->chkScaleProportional->setChecked(it.scaleProportional);
	}

	m_updatingProps = false;
}

void AwardDesignerDialog::applyPropsToItem()
{
	if (!m_selectedItem) {
		return;
	}

	AwardDesigner::Item &it = m_selectedItem->designItem();

	it.x = ui->spX->value();
	it.y = ui->spY->value();
	it.w = ui->spW->value();
	it.h = ui->spH->value();

	if (it.kind == AwardDesigner::Item::Field) {
		it.fieldId = ui->cbxItemField->currentData().toString();
		it.customText = ui->edCustomText->text();
		it.fontFamily = ui->cbxFont->currentFont().family();
		it.fontSize = ui->spFontSize->value();
		it.bold = ui->chkBold->isChecked();
		it.italic = ui->chkItalic->isChecked();
		it.color = m_colorHex;
		it.halign = ui->cbxAlign->currentData().toInt();

		// Show/hide custom text input depending on selected field
		bool is_custom = (it.fieldId == QLatin1String("customText"));
		ui->lblCustomText->setVisible(is_custom);
		ui->edCustomText->setVisible(is_custom);
	} else {
		it.imagePath = ui->edImagePath->text();
		it.scaleProportional = ui->chkScaleProportional->isChecked();
	}

	m_selectedItem->refreshFromItem();
}

void AwardDesignerDialog::updateColorButton()
{
	QPixmap pm(16, 16);
	pm.fill(QColor(m_colorHex));
	ui->btnColor->setIcon(QIcon(pm));
	ui->btnColor->setText(QStringLiteral("  ") + m_colorHex);
}

void AwardDesignerDialog::setPropsEnabled(bool enabled)
{
	ui->grpProperties->setEnabled(enabled);
}

void AwardDesignerDialog::updateItemKindVisibility(AwardDesigner::Item::Kind kind)
{
	bool is_field = (kind == AwardDesigner::Item::Field);
	ui->frameFieldContent->setVisible(is_field);
	ui->frameImageContent->setVisible(!is_field);
	ui->frameTextStyle->setVisible(is_field);
}

void AwardDesignerDialog::showEvent(QShowEvent *event)
{
	QDialog::showEvent(event);
	QTimer::singleShot(0, this, [this]() {
		ui->graphicsView->fitInView(m_scene->sceneRect(), Qt::KeepAspectRatio);
	});
}

bool AwardDesignerDialog::eventFilter(QObject *obj, QEvent *event)
{
	if (obj == ui->graphicsView && event->type() == QEvent::Wheel) {
		auto *we = static_cast<QWheelEvent *>(event);
		if (we->modifiers() & Qt::ControlModifier) {
			const double factor = we->angleDelta().y() > 0 ? 1.15 : 1.0 / 1.15;
			ui->graphicsView->scale(factor, factor);
			return true;
		}
	}
	return QDialog::eventFilter(obj, event);
}
