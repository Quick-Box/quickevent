#pragma once
#include <QByteArray>
#include <QCoreApplication>
#include <QList>
#include <QPair>
#include <QSizeF>
#include <QString>
#include <QStringList>
#include <Qt>

namespace AwardDesigner {

struct FieldDef {
	QString id;
	QString label;
};

struct Item {
	enum Kind { Image = 0, Field = 1 };
	Kind kind = Field;

	// Geometry in mm
	qreal x = 20, y = 20, w = 170, h = 15;
	int zOrder = 0;

	// Image
	QString imagePath;
	// Image bytes embedded in the design so it renders on any machine without the
	// original file. Populated from imagePath on save; imagePath is kept for the file
	// name and re-browsing.
	QByteArray imageData;

	// Field
	// Free text; `$(key)` is replaced by the award data value, `$$(` yields a literal `$(`
	QString text = QStringLiteral("$(eventName)");

	// Text formatting (used for Field items)
	QString fontFamily = QStringLiteral("Arial");
	int fontSize = 14;
	bool bold = false;
	bool italic = false;
	QString color = QStringLiteral("#000000");
	int halign = Qt::AlignHCenter; // stored as int

	// Image sizing
	bool scaleProportional = true;
};

struct Design
{
	Q_DECLARE_TR_FUNCTIONS(AwardDesigner::Design)
public:
	static QList<FieldDef> relayFields();
	static QList<FieldDef> runsFields();

	qreal pageW = 210;
	qreal pageH = 297;
	// Full-page background image drawn below all items. Bytes are embedded like Item::imageData.
	QString bgPath;
	QByteArray bgData;
	QList<Item> items;

	// Serialize the design to a self-contained, renderable Typst document. The item
	// model is embedded as `// @item ...` comment tags so the designer can reload it;
	// Typst ignores the comments and renders the document directly.
	QString toTypst() const;
	static Design fromTypst(const QString &src);

	// Absolute paths of all image items, in item order.
	QStringList imageFiles() const;

	// (file name, bytes) for every image item carrying embedded data, in item order.
	QList<QPair<QString, QByteArray>> imageBlobs() const;

	// Fill imageData from imagePath for any image item that has a readable file but no
	// embedded bytes yet, making the design self-contained.
	void embedImages();

	// Page size in mm read from a Typst award document (any .typ following the award
	// contract), falling back to A4 when it cannot be determined.
	static QSizeF pageSizeFromTypst(const QString &src);

	bool saveToDb(const QString &relative_file_name) const;
	static Design loadFile(const QString &name);
	// relative_reports_root filter: "relay", "runs", or QString() for all
	static QMap<QString, QString> listAwards(const QString &relative_reports_root);

	bool isValid() const { return !items.isEmpty(); }

	static Design defaultRelayDesign();
	static Design defaultRunsDesign();
};

std::tuple<QString, QStringList> loadTypstTemplate(const QString &path);

} // namespace AwardDesigner
