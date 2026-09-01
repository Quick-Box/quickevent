#pragma once
#include <QByteArray>
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

QList<FieldDef> relayFields();
QList<FieldDef> runsFields();

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
	QString fieldId = QStringLiteral("eventName");
	QString customText;

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

struct Design {
	QString name;
	QString type; // "relay" or "runs"; empty = treat as "relay" (backward compat)
	qreal pageW = 210;
	qreal pageH = 297;
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

	bool saveToDb() const;
	static Design loadFromDb(const QString &name);
	// type filter: "relay", "runs", or QString() for all
	static QStringList listFromDb(const QString &type = QString());
	static bool deleteFromDb(const QString &name);

	static QString dbKey(const QString &name)
	{
		return QLatin1String("awards.design.") + name;
	}

	bool isValid() const { return !name.isEmpty(); }

	static Design defaultRelayDesign();
	static Design defaultRunsDesign();
};

std::tuple<QString, QStringList> loadTypstTemplate(const QString &path);

} // namespace AwardDesigner
