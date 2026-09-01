#include "awarddesign.h"

#include <qf/core/sql/query.h>
#include <qf/core/log.h>

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QHash>
#include <QRegularExpression>
#include <QUrl>

#include <algorithm>

namespace AwardDesigner {

#define TR(s) QCoreApplication::translate("AwardDesigner", s)

QList<FieldDef> relayFields()
{
	return {
		{QStringLiteral("eventName"), TR("Název závodu")},
		{QStringLiteral("date"), TR("Datum")},
		{QStringLiteral("place"), TR("Místo konání")},
		{QStringLiteral("positionCategory"), TR("Pořadí v kategorii")},
		{QStringLiteral("position"), TR("Pořadí")},
		{QStringLiteral("category"), TR("Kategorie")},
		{QStringLiteral("clubName"), TR("Název štafety/klubu")},
		{QStringLiteral("runners"), TR("Závodníci (seznam)")},
		{QStringLiteral("mainReferee"), TR("Hlavní rozhodčí")},
		{QStringLiteral("director"), TR("Ředitel závodu")},
		{QStringLiteral("customText"), TR("Vlastní text")},
	};
}

QList<FieldDef> runsFields()
{
	return {
		{QStringLiteral("eventName"), TR("Název závodu")},
		{QStringLiteral("date"), TR("Datum")},
		{QStringLiteral("place"), TR("Místo konání")},
		{QStringLiteral("positionCategory"), TR("Pořadí v kategorii")},
		{QStringLiteral("position"), TR("Pořadí")},
		{QStringLiteral("category"), TR("Kategorie")},
		{QStringLiteral("competitorName"), TR("Jméno závodníka")},
		{QStringLiteral("clubName"), TR("Klub")},
		{QStringLiteral("mainReferee"), TR("Hlavní rozhodčí")},
		{QStringLiteral("director"), TR("Ředitel závodu")},
		{QStringLiteral("customText"), TR("Vlastní text")},
	};
}

namespace {
Item makeFieldItem(const QString &field_id, qreal x, qreal y, qreal w, qreal h,
	const QString &font_family, int font_size, bool bold,
	const QString &color = QStringLiteral("#000000"),
	int halign = Qt::AlignHCenter)
{
	Item it;
	it.kind = Item::Field;
	it.fieldId = field_id;
	it.x = x; it.y = y; it.w = w; it.h = h;
	it.fontFamily = font_family; it.fontSize = font_size; it.bold = bold;
	it.color = color; it.halign = halign;
	return it;
}
}

Design Design::defaultRelayDesign()
{
	Design d;
	d.type = QStringLiteral("relay");
	d.pageW = 210; d.pageH = 297;

	// Event name — large bold
	d.items << makeFieldItem(QStringLiteral("eventName"),
		15, 15, 180, 14, QStringLiteral("Arial"), 16, true);

	// Date — small red
	d.items << makeFieldItem(QStringLiteral("date"),
		15, 31, 180, 8, QStringLiteral("Arial"), 10, false, QStringLiteral("#cc0000"));

	// "Diplom" heading — huge Times Serif maroon
	Item diplom;
	diplom.kind = Item::Field;
	diplom.fieldId = QStringLiteral("customText");
	diplom.customText = QStringLiteral("Diplom");
	diplom.x = 15; diplom.y = 52; diplom.w = 180; diplom.h = 38;
	diplom.fontFamily = QStringLiteral("Times New Roman"); diplom.fontSize = 72;
	diplom.color = QStringLiteral("#800000"); diplom.halign = Qt::AlignHCenter;
	d.items << diplom;

	// Position + category combined
	d.items << makeFieldItem(QStringLiteral("positionCategory"),
		15, 100, 180, 12, QStringLiteral("Arial"), 16, true);

	// Club name
	d.items << makeFieldItem(QStringLiteral("clubName"),
		15, 117, 180, 12, QStringLiteral("Arial"), 16, true);

	// Runners list (multi-line, generous height)
	d.items << makeFieldItem(QStringLiteral("runners"),
		15, 133, 180, 50, QStringLiteral("Arial"), 13, false);

	// Main referee signature (left)
	d.items << makeFieldItem(QStringLiteral("mainReferee"),
		15, 265, 80, 8, QStringLiteral("Arial"), 10, false);

	// Director signature (right)
	d.items << makeFieldItem(QStringLiteral("director"),
		115, 265, 80, 8, QStringLiteral("Arial"), 10, false);

	return d;
}

Design Design::defaultRunsDesign()
{
	Design d;
	d.type = QStringLiteral("runs");
	d.pageW = 210; d.pageH = 297;

	d.items << makeFieldItem(QStringLiteral("eventName"),
		15, 15, 180, 14, QStringLiteral("Arial"), 16, true);

	d.items << makeFieldItem(QStringLiteral("date"),
		15, 31, 180, 8, QStringLiteral("Arial"), 10, false, QStringLiteral("#cc0000"));

	Item diplom;
	diplom.kind = Item::Field;
	diplom.fieldId = QStringLiteral("customText");
	diplom.customText = QStringLiteral("Diplom");
	diplom.x = 15; diplom.y = 52; diplom.w = 180; diplom.h = 38;
	diplom.fontFamily = QStringLiteral("Times New Roman"); diplom.fontSize = 72;
	diplom.color = QStringLiteral("#800000"); diplom.halign = Qt::AlignHCenter;
	d.items << diplom;

	d.items << makeFieldItem(QStringLiteral("positionCategory"),
		15, 100, 180, 12, QStringLiteral("Arial"), 16, true);

	d.items << makeFieldItem(QStringLiteral("competitorName"),
		15, 117, 180, 12, QStringLiteral("Arial"), 16, true);

	d.items << makeFieldItem(QStringLiteral("clubName"),
		15, 133, 180, 10, QStringLiteral("Arial"), 13, false);

	d.items << makeFieldItem(QStringLiteral("mainReferee"),
		15, 265, 80, 8, QStringLiteral("Arial"), 10, false);

	d.items << makeFieldItem(QStringLiteral("director"),
		115, 265, 80, 8, QStringLiteral("Arial"), 10, false);

	return d;
}

// --- Typst serialization ------------------------------------------------------
namespace {
QString escapeTypstString(const QString &s)
{
	QString out = s;
	out.replace(QLatin1Char('\\'), QStringLiteral("\\\\"));
	out.replace(QLatin1Char('"'), QStringLiteral("\\\""));
	out.replace(QLatin1Char('\n'), QStringLiteral("\\n"));
	return out;
}

QString typstAlignment(int halign)
{
	if (halign == Qt::AlignLeft)
		return QStringLiteral("left");
	if (halign == Qt::AlignRight)
		return QStringLiteral("right");
	return QStringLiteral("center");
}

// key=value tag encoding for the `// @item`/`// @design` round-trip comments.
// Values are percent-encoded so they never contain spaces or '=', keeping parsing
// a plain split. This is not JSON — the comment is inert Typst the compiler ignores.
QString enc(const QString &s)
{
	return QString::fromLatin1(QUrl::toPercentEncoding(s));
}

QString dec(const QString &s)
{
	return QString::fromUtf8(QByteArray::fromPercentEncoding(s.toLatin1()));
}

QString itemTag(const Item &it)
{
	QStringList kv;
	kv << QStringLiteral("kind=") + QString::number(it.kind);
	kv << QStringLiteral("x=") + QString::number(it.x, 'f', 3);
	kv << QStringLiteral("y=") + QString::number(it.y, 'f', 3);
	kv << QStringLiteral("w=") + QString::number(it.w, 'f', 3);
	kv << QStringLiteral("h=") + QString::number(it.h, 'f', 3);
	kv << QStringLiteral("zOrder=") + QString::number(it.zOrder);
	kv << QStringLiteral("imagePath=") + enc(it.imagePath);
	// base64 is space-free, so it survives the space-delimited tag parsing untouched
	kv << QStringLiteral("imageData=") + QString::fromLatin1(it.imageData.toBase64());
	kv << QStringLiteral("fieldId=") + enc(it.fieldId);
	kv << QStringLiteral("customText=") + enc(it.customText);
	kv << QStringLiteral("fontFamily=") + enc(it.fontFamily);
	kv << QStringLiteral("fontSize=") + QString::number(it.fontSize);
	kv << QStringLiteral("bold=") + QString::number(it.bold ? 1 : 0);
	kv << QStringLiteral("italic=") + QString::number(it.italic ? 1 : 0);
	kv << QStringLiteral("color=") + enc(it.color);
	kv << QStringLiteral("halign=") + QString::number(it.halign);
	kv << QStringLiteral("scaleProportional=") + QString::number(it.scaleProportional ? 1 : 0);
	return QStringLiteral("  // @item ") + kv.join(QLatin1Char(' ')) + QLatin1Char('\n');
}

Item itemFromTag(const QString &tag)
{
	QHash<QString, QString> m;
	const auto parts = QStringView(tag).split(QLatin1Char(' '), Qt::SkipEmptyParts);
	for (const auto &p : parts) {
		int eq = p.indexOf(QLatin1Char('='));
		if (eq < 0)
			continue;
		m.insert(p.left(eq).toString(), p.mid(eq + 1).toString());
	}
	Item it;
	it.kind = static_cast<Item::Kind>(m.value(QStringLiteral("kind")).toInt());
	it.x = m.value(QStringLiteral("x"), QStringLiteral("20")).toDouble();
	it.y = m.value(QStringLiteral("y"), QStringLiteral("20")).toDouble();
	it.w = m.value(QStringLiteral("w"), QStringLiteral("170")).toDouble();
	it.h = m.value(QStringLiteral("h"), QStringLiteral("15")).toDouble();
	it.zOrder = m.value(QStringLiteral("zOrder")).toInt();
	it.imagePath = dec(m.value(QStringLiteral("imagePath")));
	it.imageData = QByteArray::fromBase64(m.value(QStringLiteral("imageData")).toLatin1());
	it.fieldId = dec(m.value(QStringLiteral("fieldId"), QStringLiteral("eventName")));
	it.customText = dec(m.value(QStringLiteral("customText")));
	it.fontFamily = dec(m.value(QStringLiteral("fontFamily"), QStringLiteral("Arial")));
	it.fontSize = m.value(QStringLiteral("fontSize"), QStringLiteral("14")).toInt();
	it.bold = m.value(QStringLiteral("bold")).toInt() != 0;
	it.italic = m.value(QStringLiteral("italic")).toInt() != 0;
	it.color = dec(m.value(QStringLiteral("color"), QStringLiteral("#000000")));
	it.halign = m.value(QStringLiteral("halign"), QString::number(Qt::AlignHCenter)).toInt();
	it.scaleProportional = m.value(QStringLiteral("scaleProportional"), QStringLiteral("1")).toInt() != 0;
	return it;
}

QString itemToTypstSnippet(const Item &item, int index)
{
	const QString dx = QString::number(item.x, 'f', 3) + QStringLiteral("mm");
	const QString dy = QString::number(item.y, 'f', 3) + QStringLiteral("mm");
	const QString w = QString::number(item.w, 'f', 3) + QStringLiteral("mm");
	const QString h = QString::number(item.h, 'f', 3) + QStringLiteral("mm");

	QString snippet = itemTag(item);

	if (item.kind == Item::Image) {
		if (item.imagePath.isEmpty())
			return snippet;
		const QString file_name = QFileInfo(item.imagePath).fileName();
		const QString fit = item.scaleProportional ? QStringLiteral("contain") : QStringLiteral("stretch");
		snippet += QStringLiteral("  #place(dx: ") + dx + QStringLiteral(", dy: ") + dy
			+ QStringLiteral(", box(width: ") + w + QStringLiteral(", height: ") + h
			+ QStringLiteral(", image(\"") + escapeTypstString(file_name)
			+ QStringLiteral("\", width: 100%, height: 100%, fit: \"") + fit
			+ QStringLiteral("\")))\n");
		return snippet;
	}

	const QString value_expr = (item.fieldId == QLatin1String("customText"))
		? QLatin1Char('"') + escapeTypstString(item.customText) + QLatin1Char('"')
		: QStringLiteral("page.at(\"") + escapeTypstString(item.fieldId) + QStringLiteral("\", default: \"\")");

	const QString var_name = QStringLiteral("val%1").arg(index);
	const QString align = typstAlignment(item.halign);
	const QString weight = item.bold ? QStringLiteral("bold") : QStringLiteral("regular");
	const QString style = item.italic ? QStringLiteral("italic") : QStringLiteral("normal");

	snippet += QStringLiteral("  #let ") + var_name + QStringLiteral(" = ") + value_expr + QStringLiteral("\n");
	snippet += QStringLiteral("  #if ") + var_name + QStringLiteral(" != \"\" [\n");
	snippet += QStringLiteral("    #place(dx: ") + dx + QStringLiteral(", dy: ") + dy
		+ QStringLiteral(", box(width: ") + w + QStringLiteral(", height: ") + h
		+ QStringLiteral(", align(") + align + QStringLiteral(" + horizon, text(font: \"")
		+ escapeTypstString(item.fontFamily) + QStringLiteral("\", size: ") + QString::number(item.fontSize)
		+ QStringLiteral("pt, weight: \"") + weight + QStringLiteral("\", style: \"") + style
		+ QStringLiteral("\", fill: rgb(\"") + escapeTypstString(item.color)
		+ QStringLiteral("\"))[#(") + var_name + QStringLiteral(".split(\"\\n\").join(linebreak()))]))) \n");
	snippet += QStringLiteral("  ]\n");
	return snippet;
}
}
QString Design::toTypst() const
{
	QList<Item> sorted = items;
	std::stable_sort(sorted.begin(), sorted.end(),
		[](const Item &a, const Item &b) { return a.zOrder < b.zOrder; });

	QString src;
	src += QStringLiteral("// @design type=") + enc(type.isEmpty() ? QStringLiteral("relay") : type)
		+ QStringLiteral(" pageW=") + QString::number(pageW, 'f', 3)
		+ QStringLiteral(" pageH=") + QString::number(pageH, 'f', 3) + QLatin1Char('\n');
	src += QStringLiteral("#set page(width: ") + QString::number(pageW, 'f', 3)
		+ QStringLiteral("mm, height: ") + QString::number(pageH, 'f', 3)
		+ QStringLiteral("mm, margin: 0mm)\n");
	src += QStringLiteral("#let pages = json(\"data.json\")\n");
	src += QStringLiteral("#for (i, page) in pages.enumerate() [\n");
	for (int idx = 0; idx < sorted.size(); ++idx)
		src += itemToTypstSnippet(sorted.at(idx), idx);
	src += QStringLiteral("  #if i < pages.len() - 1 [#pagebreak()]\n");
	src += QStringLiteral("]\n");
	return src;
}

Design Design::fromTypst(const QString &src)
{
	Design d;
	QSizeF sz = pageSizeFromTypst(src);
	d.pageW = sz.width();
	d.pageH = sz.height();

	static const QRegularExpression re_design(QStringLiteral("^// @design (.+)$"),
		QRegularExpression::MultilineOption);
	auto dm = re_design.match(src);
	if (dm.hasMatch()) {
		for (const auto &p : dm.captured(1).split(QLatin1Char(' '), Qt::SkipEmptyParts)) {
			int eq = p.indexOf(QLatin1Char('='));
			if (eq > 0 && p.left(eq) == QLatin1String("type"))
				d.type = dec(p.mid(eq + 1));
		}
	}
	if (d.type.isEmpty())
		d.type = QStringLiteral("relay");

	static const QRegularExpression re_item(QStringLiteral("^\\s*// @item (.+)$"),
		QRegularExpression::MultilineOption);
	auto it = re_item.globalMatch(src);
	while (it.hasNext())
		d.items.append(itemFromTag(it.next().captured(1)));
	return d;
}

QStringList Design::imageFiles() const
{
	QStringList files;
	for (const auto &item : items) {
		if (item.kind == Item::Image && !item.imagePath.isEmpty())
			files << item.imagePath;
	}
	return files;
}

QList<QPair<QString, QByteArray>> Design::imageBlobs() const
{
	QList<QPair<QString, QByteArray>> blobs;
	for (const auto &item : items) {
		if (item.kind == Item::Image && !item.imageData.isEmpty())
			blobs << qMakePair(QFileInfo(item.imagePath).fileName(), item.imageData);
	}
	return blobs;
}

void Design::embedImages()
{
	for (auto &item : items) {
		if (item.kind != Item::Image || !item.imageData.isEmpty() || item.imagePath.isEmpty())
			continue;
		QFile f(item.imagePath);
		if (f.open(QIODevice::ReadOnly))
			item.imageData = f.readAll();
	}
}

QSizeF Design::pageSizeFromTypst(const QString &src)
{
	// Prefer the explicit designer header, fall back to the `#set page(...)` declaration.
	static const QRegularExpression re_hdr(
		QStringLiteral("pageW=([0-9.]+)\\s+pageH=([0-9.]+)"));
	auto hm = re_hdr.match(src);
	if (hm.hasMatch())
		return QSizeF(hm.captured(1).toDouble(), hm.captured(2).toDouble());

	static const QRegularExpression re_page(
		QStringLiteral("width:\\s*([0-9.]+)mm[^)]*height:\\s*([0-9.]+)mm"));
	auto pm = re_page.match(src);
	if (pm.hasMatch())
		return QSizeF(pm.captured(1).toDouble(), pm.captured(2).toDouble());

	return QSizeF(210, 297); // A4
}

bool Design::saveToDb() const
{
	if (name.isEmpty()) {
		qfWarning() << "Design name is empty, cannot save to DB";
		return false;
	}
	QString key = dbKey(name);
	Design self = *this;
	self.embedImages();
	QString typ = self.toTypst();
	qf::core::sql::Query q_up;
	q_up.prepare(QStringLiteral("UPDATE reports SET data=:data WHERE path=:path"));
	q_up.bindValue(QStringLiteral(":path"), key);
	q_up.bindValue(QStringLiteral(":data"), typ.toUtf8());
	if (!q_up.exec()) {
		qfWarning() << "Failed to update award design in DB:" << q_up.lastErrorText();
		return false;
	}
	if (q_up.numRowsAffected() < 1) {
		qf::core::sql::Query q_ins;
		q_ins.prepare(QStringLiteral("INSERT INTO reports(path, data) VALUES(:path, :data)"));
		q_ins.bindValue(QStringLiteral(":path"), key);
		q_ins.bindValue(QStringLiteral(":data"), typ.toUtf8());
		if (!q_ins.exec()) {
			qfWarning() << "Failed to insert award design into DB:" << q_ins.lastErrorText();
			return false;
		}
	}
	return true;
}

Design Design::loadFromDb(const QString &name)
{
	qf::core::sql::Query q;
	q.prepare(QStringLiteral("SELECT data FROM reports WHERE path=:path"));
	q.bindValue(QStringLiteral(":path"), dbKey(name));
	q.exec();
	if (q.next()) {
		Design d = fromTypst(QString::fromUtf8(q.value(0).toByteArray()));
		d.name = name;
		return d;
	}
	return Design{};
}

QStringList Design::listFromDb(const QString &type)
{
	qf::core::sql::Query q;
	q.prepare(QStringLiteral("SELECT path, data FROM reports WHERE path LIKE 'awards.design.%' ORDER BY path"));
	q.exec();
	QStringList names;
	const int prefix_len = QStringLiteral("awards.design.").length();
	while (q.next()) {
		if (!type.isEmpty()) {
			// default "relay" when no header (backward compat / hand-written)
			QString t = fromTypst(QString::fromUtf8(q.value(1).toByteArray())).type;
			if (t != type)
				continue;
		}
		names << q.value(0).toString().mid(prefix_len);
	}
	return names;
}

bool Design::deleteFromDb(const QString &name)
{
	qf::core::sql::Query q;
	q.prepare(QStringLiteral("DELETE FROM reports WHERE path=:path"));
	q.bindValue(QStringLiteral(":path"), dbKey(name));
	q.exec();
	return q.numRowsAffected() > 0;
}

std::tuple<QString, QStringList> loadTypstTemplate(const QString &path)
{
	QFile f(path);
	if (!f.open(QIODevice::ReadOnly)) {
		qfWarning() << "Cannot open Typst award template:" << path;
		return {};
	}
	QString out_source = QString::fromUtf8(f.readAll());

	QStringList out_image_files;
	QDir images_dir(QFileInfo(path).absolutePath() + QStringLiteral("/images"));
	if (images_dir.exists()) {
		const auto entries = images_dir.entryInfoList(QDir::Files, QDir::Name);
		for (const QFileInfo &fi : entries)
			out_image_files << fi.absoluteFilePath();
	}
	return { out_source, out_image_files };
}

} // namespace AwardDesigner
