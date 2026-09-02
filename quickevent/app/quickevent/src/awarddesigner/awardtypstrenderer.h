#pragma once
#include "awarddesign.h"

#include <QByteArray>
#include <QImage>
#include <QList>
#include <QPair>
#include <QString>
#include <QStringList>
#include <QVariantMap>

class QTemporaryDir;

namespace qf::core::utils { class TreeTable; }
namespace Event { class EventConfig; }

class AwardTypstRenderer
{
public:
	// Render an arbitrary award .typ document. image_files are absolute paths copied
	// next to the document under their file name (templates reference them by name).
	explicit AwardTypstRenderer(QString typ_source, QStringList image_files = {});
	// Convenience: render a designer design (serialized to .typ).
	explicit AwardTypstRenderer(const AwardDesigner::Design &design);

	// Collect per-relay data maps from the relay results TreeTable
	QList<QVariantMap> collectPages(const qf::core::utils::TreeTable &tt,
									const Event::EventConfig &event_config) const;

	// Collect per-runner data maps from the stage results TreeTable
	QList<QVariantMap> collectRunsPagesData(const qf::core::utils::TreeTable &tt,
		const Event::EventConfig &event_config) const;

	// Render pages to a QImage per page, via a generated Typst document rendered at the given PPI
	QList<QImage> renderToImages(const QList<QVariantMap> &pages, int dpi = 96) const;

	// Render the whole document to a single PDF file; returns the file path, empty on failure.
	// The file lives in a temporary directory whose lifetime is tied to the given holder.
	QString renderToPdf(const QList<QVariantMap> &pages, QTemporaryDir &out_dir) const;

	// Render the whole document to one SVG document per page; empty list on failure.
	QList<QByteArray> renderToSvgs(const QList<QVariantMap> &pages) const;

private:
	QString m_typSource;
	QStringList m_imageFiles;
	// (file name, bytes) for images embedded in a design; written to the render dir
	// before falling back to copying m_imageFiles by path.
	QList<QPair<QString, QByteArray>> m_imageBlobs;

	// Compile the document for all pages into out_dir/out-{p}.<ext>. Returns true on success.
	bool compile(const QList<QVariantMap> &pages, const QString &format,
		const QString &out_pattern, QTemporaryDir &out_dir, int dpi = 0) const;

	void copyImageFilesToDir(const QString &dir_path) const;
};
