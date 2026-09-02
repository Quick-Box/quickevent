#include "awardtypstrenderer.h"
#include "typstexecutable.h"

#include <plugins/Event/src/eventconfig.h>

#include <qf/core/utils/treetable.h>
#include <qf/core/log.h>

#include <QDate>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QProcess>
#include <QSet>
#include <QTemporaryDir>

#include <utility>

AwardTypstRenderer::AwardTypstRenderer(QString typ_source, QStringList image_files)
	: m_typSource(std::move(typ_source))
	, m_imageFiles(std::move(image_files))
{
}

AwardTypstRenderer::AwardTypstRenderer(const AwardDesigner::Design &design)
	: m_typSource(design.toTypst())
	, m_imageFiles(design.imageFiles())
	, m_imageBlobs(design.imageBlobs())
{
}

QList<QVariantMap> AwardTypstRenderer::collectPages(const qf::core::utils::TreeTable &tt,
	const Event::EventConfig &event_config) const
{
	QList<QVariantMap> pages;

	QVariantMap event_map = tt.value(QStringLiteral("event")).toMap();
	QString event_name = event_map.value(QStringLiteral("name")).toString();
	QString event_place = event_map.value(QStringLiteral("place")).toString();

	QString date_str;
	{
		QDateTime dt;
		QVariant stage_start = tt.value(QStringLiteral("stageStart"));
		if (stage_start.isValid())
			dt = stage_start.toDateTime();
		if (!dt.isValid()) {
			QVariant ev_dt = event_map.value(QStringLiteral("dateTime"));
			if (!ev_dt.isValid())
				ev_dt = event_map.value(QStringLiteral("date"));
			dt = ev_dt.toDateTime();
			if (!dt.isValid())
				dt = QDateTime(ev_dt.toDate(), QTime());
		}
		if (dt.isValid())
			date_str = dt.toString(QStringLiteral("dd.MM.yyyy"));
	}

	QString main_referee;
	QString director;
	main_referee = event_config.mainReferee;
	director = event_config.director;

	for (int ci = 0; ci < tt.rowCount(); ++ci) {
		auto class_row = tt.row(ci);
		QString class_name = class_row.value(QStringLiteral("className")).toString();
		auto relay_table = class_row.table(0);

		for (int ri = 0; ri < relay_table.rowCount(); ++ri) {
			auto relay_row = relay_table.row(ri);
			int pos = relay_row.value(QStringLiteral("pos")).toInt();
			QString org_name = relay_row.value(QStringLiteral("orgName")).toString();
			if (org_name.isEmpty())
				org_name = relay_row.value(QStringLiteral("name")).toString();

			QStringList runner_names;
			auto runner_table = relay_row.table(0);
			for (int ki = 0; ki < runner_table.rowCount(); ++ki) {
				auto runner_row = runner_table.row(ki);
				QString name = runner_row.value(QStringLiteral("competitorName")).toString();
				if (!name.isEmpty())
					runner_names << name;
			}

			const QString pos_str = (pos > 0)
				? QString::number(pos) + QStringLiteral(". místo")
				: QStringLiteral("místo");

			QVariantMap data;
			data[QStringLiteral("pos")] = pos; // raw integer for keying
			data[QStringLiteral("eventName")] = event_name;
			data[QStringLiteral("date")] = date_str;
			data[QStringLiteral("place")] = event_place;
			data[QStringLiteral("position")] = pos_str;
			data[QStringLiteral("category")] = class_name;
			data[QStringLiteral("positionCategory")] = pos_str + QStringLiteral(" v kategorii ") + class_name;
			data[QStringLiteral("clubName")] = org_name;
			data[QStringLiteral("runners")] = runner_names.join(QStringLiteral("\n"));
			data[QStringLiteral("mainReferee")] = main_referee;
			data[QStringLiteral("director")] = director;
			pages.append(data);
		}
	}
	return pages;
}

QList<QVariantMap> AwardTypstRenderer::collectRunsPagesData(const qf::core::utils::TreeTable &tt,
	const Event::EventConfig &event_config) const
{
	QList<QVariantMap> pages;

	QVariantMap event_map = tt.value(QStringLiteral("event")).toMap();
	QString event_name = event_map.value(QStringLiteral("name")).toString();
	QString event_place = event_map.value(QStringLiteral("place")).toString();

	QString date_str;
	{
		QDateTime dt;
		QVariant stage_start = tt.value(QStringLiteral("stageStart"));
		if (stage_start.isValid())
			dt = stage_start.toDateTime();
		if (!dt.isValid()) {
			QVariant ev_dt = event_map.value(QStringLiteral("dateTime"));
			if (!ev_dt.isValid())
				ev_dt = event_map.value(QStringLiteral("date"));
			dt = ev_dt.toDateTime();
			if (!dt.isValid())
				dt = QDateTime(ev_dt.toDate(), QTime());
		}
		if (dt.isValid())
			date_str = dt.toString(QStringLiteral("dd.MM.yyyy"));
	}

	QString main_referee;
	QString director;
	main_referee = event_config.mainReferee;
	director = event_config.director;

	for (int ci = 0; ci < tt.rowCount(); ++ci) {
		auto class_row = tt.row(ci);
		QString class_name = class_row.value(QStringLiteral("name")).toString();
		auto runner_table = class_row.table(0);

		for (int ri = 0; ri < runner_table.rowCount(); ++ri) {
			auto runner_row = runner_table.row(ri);

			// stageResultsTable has integer "npos"; nstagesResultsTable has text "pos" like "1."
			int pos = runner_row.value(QStringLiteral("npos")).toInt();
			if (pos <= 0) {
				QString pos_text = runner_row.value(QStringLiteral("pos")).toString();
				if (pos_text.endsWith(QLatin1Char('.')))
					pos = pos_text.chopped(1).toInt();
			}

			QString competitor_name = runner_row.value(QStringLiteral("competitorName")).toString();
			QString org_name = runner_row.value(QStringLiteral("clubs.name")).toString();
			if (org_name.isEmpty())
				org_name = runner_row.value(QStringLiteral("club")).toString();

			const QString pos_str = (pos > 0)
				? QString::number(pos) + QStringLiteral(". místo")
				: QString();

			QVariantMap data;
			data[QStringLiteral("_classIdx")] = ci;
			data[QStringLiteral("_runnerIdx")] = ri;
			data[QStringLiteral("pos")] = pos;
			data[QStringLiteral("eventName")] = event_name;
			data[QStringLiteral("date")] = date_str;
			data[QStringLiteral("place")] = event_place;
			data[QStringLiteral("position")] = pos_str;
			data[QStringLiteral("category")] = class_name;
			data[QStringLiteral("positionCategory")] = pos_str.isEmpty()
				? QStringLiteral("v kategorii ") + class_name
				: pos_str + QStringLiteral(" v kategorii ") + class_name;
			data[QStringLiteral("competitorName")] = competitor_name;
			data[QStringLiteral("clubName")] = org_name;
			data[QStringLiteral("mainReferee")] = main_referee;
			data[QStringLiteral("director")] = director;
			pages.append(data);
		}
	}
	return pages;
}

void AwardTypstRenderer::copyImageFilesToDir(const QString &dir_path) const
{
	QSet<QString> copied; // by file name, to avoid duplicate copies / collisions

	// Embedded image bytes are self-contained; write them first so a design renders
	// even when the original file is missing on this machine.
	for (const auto &blob : m_imageBlobs) {
		const QString &file_name = blob.first;
		if (file_name.isEmpty() || copied.contains(file_name))
			continue;
		QFile out(dir_path + QLatin1Char('/') + file_name);
		if (out.open(QIODevice::WriteOnly) && out.write(blob.second) == blob.second.size())
			copied.insert(file_name);
		else
			qfWarning() << "Cannot write embedded award image" << file_name << "for Typst rendering";
	}

	for (const QString &src : m_imageFiles) {
		if (src.isEmpty())
			continue;
		const QString file_name = QFileInfo(src).fileName();
		if (copied.contains(file_name))
			continue;
		if (QFile::copy(src, dir_path + QLatin1Char('/') + file_name))
			copied.insert(file_name);
		else
			qfWarning() << "Cannot copy award image" << src << "for Typst rendering";
	}
}

bool AwardTypstRenderer::compile(const QList<QVariantMap> &pages, const QString &format,
	const QString &out_pattern, QTemporaryDir &out_dir, int dpi) const
{
	if (pages.isEmpty())
		return false;

	const QString typst = Typst::executablePath();
	if (typst.isEmpty()) {
		qfWarning() << "typst executable not found, cannot render awards";
		return false;
	}

	if (!out_dir.isValid()) {
		qfWarning() << "Cannot create a temporary directory for Typst award rendering";
		return false;
	}

	copyImageFilesToDir(out_dir.path());

	QJsonArray pages_json;
	for (const auto &page : pages)
		pages_json.append(QJsonObject::fromVariantMap(page));
	{
		QFile data_file(out_dir.filePath(QStringLiteral("data.json")));
		if (!data_file.open(QIODevice::WriteOnly)) {
			qfWarning() << "Cannot write Typst award data file";
			return false;
		}
		data_file.write(QJsonDocument(pages_json).toJson(QJsonDocument::Compact));
	}

	const QString typ_file_path = out_dir.filePath(QStringLiteral("award.typ"));
	{
		QFile typ_file(typ_file_path);
		if (!typ_file.open(QIODevice::WriteOnly)) {
			qfWarning() << "Cannot write Typst award source file";
			return false;
		}
		typ_file.write(m_typSource.toUtf8());
	}

	QStringList args;
	args << QStringLiteral("compile") << QStringLiteral("award.typ") << out_pattern
		<< QStringLiteral("--format") << format;
	if (dpi > 0)
		args << QStringLiteral("--ppi") << QString::number(dpi);

	QProcess process;
	process.setWorkingDirectory(out_dir.path());
	process.start(typst, args);
	if (!process.waitForFinished(30000) || process.exitCode() != 0) {
		qfWarning() << "typst compile failed:" << process.errorString()
			<< QString::fromUtf8(process.readAllStandardError());
		return false;
	}
	return true;
}

QList<QImage> AwardTypstRenderer::renderToImages(const QList<QVariantMap> &pages, int dpi) const
{
	QTemporaryDir temp_dir;
	if (!compile(pages, QStringLiteral("png"), QStringLiteral("out-{p}.png"), temp_dir, dpi))
		return {};

	QList<QImage> images;
	images.reserve(pages.size());
	for (int i = 1; i <= pages.size(); ++i) {
		QImage img(temp_dir.filePath(QStringLiteral("out-%1.png").arg(i)));
		if (img.isNull()) {
			qfWarning() << "Typst did not produce page" << i << "of the award document";
			continue;
		}
		images.append(std::move(img));
	}
	return images;
}

QString AwardTypstRenderer::renderToPdf(const QList<QVariantMap> &pages, QTemporaryDir &out_dir) const
{
	if (!compile(pages, QStringLiteral("pdf"), QStringLiteral("award.pdf"), out_dir))
		return {};

	const QString pdf_path = out_dir.filePath(QStringLiteral("award.pdf"));
	if (!QFile::exists(pdf_path)) {
		qfWarning() << "Typst did not produce the award PDF document";
		return {};
	}
	return pdf_path;
}

QList<QByteArray> AwardTypstRenderer::renderToSvgs(const QList<QVariantMap> &pages) const
{
	QTemporaryDir temp_dir;
	if (!compile(pages, QStringLiteral("svg"), QStringLiteral("out-{p}.svg"), temp_dir))
		return {};

	QList<QByteArray> svgs;
	svgs.reserve(pages.size());
	for (int i = 1; i <= pages.size(); ++i) {
		QFile svg_file(temp_dir.filePath(QStringLiteral("out-%1.svg").arg(i)));
		if (!svg_file.open(QIODevice::ReadOnly)) {
			qfWarning() << "Typst did not produce page" << i << "of the award document";
			continue;
		}
		svgs.append(svg_file.readAll());
	}
	return svgs;
}
