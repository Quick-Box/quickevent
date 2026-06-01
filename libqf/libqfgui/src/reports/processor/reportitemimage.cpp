#include "reportitemimage.h"

#include "reportprocessor.h"
#include "reportpainter.h"

#include <qf/core/log.h>
#include <qf/core/assert.h>
#include <qf/core/string.h>
#include <qf/core/utils/fileutils.h>

#include <QCryptographicHash>
#include <QSvgRenderer>
#include <QBuffer>

namespace qfu = qf::core::utils;
using namespace qf::gui::reports;

//===============================================================
//                ReportItemImage
//===============================================================
/*--
bool ReportItemImage::childrenSynced()
{
	return childrenSyncedFlag;
}

void ReportItemImage::syncChildren()
{
	//qfDebug() << QF_FUNC_NAME << element.tagName() << "id:" << element.attribute("id");
	QString orig_src = element.attribute("src");
	QString processor_img_key;
	src = orig_src;
	ReportItem::Image im;
	if(orig_src.startsWith("key:/")) {
		/// obrazek je ocekavan v processor()->images(), takze neni treba delat nic
		src = QString();
	}
	else if(orig_src.isEmpty()) {
		/// obrazek bude v datech
		src = QString();
	}
	if(!src.isEmpty()) {
		if(src.startsWith("./") || src.startsWith("../")) {
			src = QFFileUtils::joinPath(QFFileUtils::path(processor()->report().fileName()), src);
			//qfInfo() << "relative path joined to:" << src;
		}
		src = processor()->searchDirs()->findFile(src);
		if(src.isEmpty()) {
			qfWarning().noSpace() << "file '" << orig_src << "' not found. Report file name: " << processor()->report().fileName();
			/// pridej fake para element, aby se jmeno chybejiciho souboru zobrazilo v reportu
			if(fakeLoadErrorPara.isNull()) {
				//qfInfo() << "creating fakeLoadErrorPara:" << orig_src;
				fakeLoadErrorPara = fakeLoadErrorParaDocument.createElement("para");
				fakeLoadErrorParaDocument.appendChild(fakeLoadErrorPara);
				fakeLoadErrorPara.setAttribute("__fake", 1);
				fakeLoadErrorPara.appendChild(fakeLoadErrorPara.ownerDocument().createTextNode(orig_src));
				processor()->createProcessibleItem(fakeLoadErrorPara, this);
				//qfInfo() << "children cnt:" << this->itemCount();
				//qfInfo() << "this:" << this << "itemCount" << itemCount() << "\n" << toString();
			}
		}
	}

	qfDebug() << "orig_src:" << orig_src;
	qfDebug() << "src:" << src;
	if(!src.isEmpty()) {
		if(src.endsWith(".svg", Qt::CaseInsensitive)) {
			QSvgRenderer ren;
			if(!ren.load(processor()->searchDirs()->loadFile(src))) qfWarning() << "SVG data read error src:" << src;
			else {
				//qfInfo() << "default size::" << ren.defaultSize().width() << ren.defaultSize().height();
				QPicture pic;
				QPainter painter(&pic);
				ren.render(&painter);
				painter.end();
				im.picture = pic;
				//qfInfo() << "bounding rect:" << Rect(pic.boundingRect()).toString();
			}
		}
		else {
			QByteArray ba = processor()->searchDirs()->loadFile(src);
			if(!im.image.loadFromData(ba)) {
				qfWarning() << "ERROR load image:" << src;
			}
			//im.image = QImage(src);
		}
		if(!im.isNull()) processor_img_key = orig_src;
	}
	if(im.isNull() && !orig_src.isEmpty()) {
		/// pridej fake para element, aby se jmeno chybejiciho souboru zobrazilo v reportu
		qfWarning().noSpace() << "QImage('" << src << "') constructor error.";
		if(fakeLoadErrorPara.isNull()) {
			fakeLoadErrorPara = fakeLoadErrorParaDocument.createElement("para");
			fakeLoadErrorParaDocument.appendChild(fakeLoadErrorPara);
			fakeLoadErrorPara.setAttribute("__fake", 1);
			fakeLoadErrorPara.appendChild(fakeLoadErrorPara.ownerDocument().createTextNode("QImage('" + src + "') constructor error."));
			processor()->createProcessibleItem(fakeLoadErrorPara, this);
		}
		src = QString();
	}
	else {
		processor()->addImage(processor_img_key, ReportItem::Image(im));
		src = processor_img_key;
	}
	qfDebug() << "src:" << src;
	//ReportItem::syncChildren();
	childrenSyncedFlag = true;
}
--*/
ReportItemImage::ReportItemImage(ReportItem *parent)
	: Super(parent)
{
	/*
	connect(this, &ReportItemImage::dataSourceChanged, this, [this](const QString &ds) {
		this->m_currentDataSource = ds;
	});
	*/
}

void ReportItemImage::updateResolvedDataSource()
{
	QString data_source = dataSource();
	if(m_currentDataSource == data_source)
		return;

	m_currentDataSource = data_source;
	QString processor_img_key;
	m_resolvedDataSource = data_source;
	ReportItem::Image im;
	if(data_source.startsWith("key:/")) {
		/// obrazek je ocekavan v processor()->images(), takze neni treba delat nic
	}
	else if(data_source.isEmpty()) {
		/// obrazek bude v datech
		m_resolvedDataSource = QString();
	}
	if(!m_resolvedDataSource.isEmpty()) {
		if(m_resolvedDataSource.startsWith("./") || m_resolvedDataSource.startsWith("../")) {
			m_resolvedDataSource = qfu::FileUtils::joinPath(qfu::FileUtils::path(processor()->reportUrl().toString()), m_resolvedDataSource);
			static const auto FILE_SCHEME = QStringLiteral("file:");
			if(m_resolvedDataSource.startsWith(FILE_SCHEME)) {
				// path like file:/path/to/file.svg is not recognized by QSvgRenderer
				m_resolvedDataSource = m_resolvedDataSource.mid(FILE_SCHEME.length());
			}
		}
		/*--
		m_resolvedDataSource = processor()->searchDirs()->findFile(m_resolvedDataSource);
		if(m_resolvedDataSource.isEmpty()) {
			qfWarning().noSpace() << "file '" << data_source << "' not found. Report file name: " << processor()->report().fileName();
			/// pridej fake para element, aby se jmeno chybejiciho souboru zobrazilo v reportu
			if(fakeLoadErrorPara.isNull()) {
				//qfInfo() << "creating fakeLoadErrorPara:" << data_source;
				fakeLoadErrorPara = fakeLoadErrorParaDocument.createElement("para");
				fakeLoadErrorParaDocument.appendChild(fakeLoadErrorPara);
				fakeLoadErrorPara.setAttribute("__fake", 1);
				fakeLoadErrorPara.appendChild(fakeLoadErrorPara.ownerDocument().createTextNode(data_source));
				processor()->createProcessibleItem(fakeLoadErrorPara, this);
				//qfInfo() << "children cnt:" << this->itemCount();
				//qfInfo() << "this:" << this << "itemCount" << itemCount() << "\n" << toString();
			}
		}
		--*/
	}

	qfDebug() << "data_source:" << data_source;
	qfDebug() << "m_resolvedDataSource:" << m_resolvedDataSource;
	if(m_resolvedDataSource.startsWith("qrc"))
		m_resolvedDataSource = m_resolvedDataSource.mid(3);
	if(!m_resolvedDataSource.isEmpty()) {
		if(m_resolvedDataSource.endsWith(".svg", Qt::CaseInsensitive)) {
			QSvgRenderer ren;
			if(!ren.load(m_resolvedDataSource)) {
				qfWarning() << "SVG data read error m_resolvedDataSource:" << m_resolvedDataSource;
			}
			else {
				//qfInfo() << "default size::" << ren.defaultSize().width() << ren.defaultSize().height();
				QPicture pic;
				QPainter painter(&pic);
				ren.render(&painter);
				painter.end();
				im.picture = pic;
				//qfInfo() << "bounding rect:" << Rect(pic.boundingRect()).toString();
			}
		}
		else {
			if(!im.image.load(m_resolvedDataSource)) {
				qfWarning() << "ERROR load image:" << m_resolvedDataSource;
			}
		}
		if(!im.isNull())
			processor_img_key = data_source;
	}
	if(im.isNull() && !data_source.isEmpty()) {
		/// pridej fake para element, aby se jmeno chybejiciho souboru zobrazilo v reportu
		qfWarning().nospace() << "QImage('" << m_resolvedDataSource << "') constructor error.";
		/*--
		if(fakeLoadErrorPara.isNull()) {
			fakeLoadErrorPara = fakeLoadErrorParaDocument.createElement("para");
			fakeLoadErrorParaDocument.appendChild(fakeLoadErrorPara);
			fakeLoadErrorPara.setAttribute("__fake", 1);
			fakeLoadErrorPara.appendChild(fakeLoadErrorPara.ownerDocument().createTextNode("QImage('" + m_resolvedDataSource + "') constructor error."));
			processor()->createProcessibleItem(fakeLoadErrorPara, this);
		}
		--*/
		m_resolvedDataSource = QString();
	}
	else {
		processor()->addImage(processor_img_key, ReportItem::Image(im));
		m_resolvedDataSource = processor_img_key;
	}
	qfDebug() << "m_resolvedDataSource:" << m_resolvedDataSource;
	//ReportItem::syncChildren();
	//--childrenSyncedFlag = true;
}

ReportItem::PrintResult ReportItemImage::printMetaPaint(QPaintDevice *paint_device, ReportItemMetaPaint* out, const ReportItem::Rect& bounding_rect)
{
	updateResolvedDataSource();
	ReportItem::PrintResult ret = Super::printMetaPaint(paint_device, out, bounding_rect);
	/*--
	ReportItemMetaPaint *mpi = out->lastChild();
	if(mpi) {
		ReportItemMetaPaintImage *img = qobject_cast<ReportItemMetaPaintImage*>(mpi->lastChild());
		if(img) {
			/// pokud se obrazek vytiskl a je to background, nastav tento flag jeho rodicovskemu frame
			if(QString(elementAttribute("backgroundItem")).toBool()) {
				mpi->renderedRect.flags |= Rect::BackgroundItem;
				//img->renderedRect.setWidth(0);
				//img->renderedRect.setHeight(0);
				//qfWarning() << mpi << "setting backgroundItem" << (mpi->renderedRect.flags & Rect::BackgroundItem);
			}
			//qfInfo() << elementAttribute("backgroundItem") << "fs:" << QString(elementAttribute("backgroundItem")).toBool() << "rendered rect:" << img->renderedRect.toString() << "image size:" << im.size().width() << "x" << im.size().height();
		}
	}
	--*/
	return ret;
}

ReportItemImage::PrintResult ReportItemImage::printMetaPaintChildren(QPaintDevice *paint_device, ReportItemMetaPaint *out, const ReportItem::Rect &bounding_rect)
{
	Q_UNUSED(paint_device);
	//qfDebug().color(QFLog::Magenta) << QF_FUNC_NAME << element.tagName() << "id:" << element.attribute("id");
	//qfDebug() << "\tbounding_rect:" << bounding_rect.toString();
	PrintResult res = PrintResult::createPrintFinished();
	Rect br = bounding_rect;
	/*--
	if(!fakeLoadErrorPara.isNull()) {
		/// src nebyl nalezen, child je para, kde je uvedeno, jak se jmenoval nenalezeny obrazek
		//qfInfo() << "this:" << this << "itemCount" << itemCount() << "\n" << toString();
		ReportItemFrame::printMetaPaintChildren(out, bounding_rect);
	}
	else
	--*/
	{
		QString src = dataSource();
		ReportItem::Image im = processor()->images().value(src);
		if(src.isEmpty()) {
			/// muze byt jeste v datech, zkus ho nahrat pro aktualni radek
			QString data_s = data();
			QByteArray img_data;
			{
				DataEncoding encoding = dataEncoding();
				if(encoding == EncodingBase64) {
					img_data = QByteArray::fromBase64(data_s.toLatin1());
				}
				else if(encoding == EncodingHex) {
					img_data = QByteArray::fromHex(data_s.toLatin1());
				}
				else {
					qfError() << "Need dataEncoding property filled to decode data.";
				}
			}
			{
				DataCompression compression = dataCompression();
				if(compression == CompressionQCompress) {
					img_data = qUncompress(img_data);
				}
			}
			if(!img_data.isEmpty()) {
				DataFormat format = dataFormat();
				if(format == FormatSvg) {
					QSvgRenderer ren;
					if(!ren.load(img_data)) {
						qfWarning() << "SVG data read error, format:" << format;
					}
					else {
						QPicture pic;
						QPainter painter(&pic);
						ren.render(&painter);
						painter.end();
						im.picture = pic;
					}
				}
				else if(format == FormatQPicture) {
					QBuffer buff(&img_data);
					QPicture pic;
					buff.open(QIODevice::ReadOnly);
					pic.load(&buff);
					//qfInfo() << "PIC size:" << Rect(pic.boundingRect()).toString();
					im.picture = pic;
				}
				else {
					const char *fmt = nullptr;
					if(format == FormatJpg)
						fmt = "jpg";
					else if(format == FormatPng)
						fmt = "png";
					if(!im.image.loadFromData(img_data, fmt))
						qfWarning() << "Image data read error, format:" << format;
				}
			}
		}
		if(im.isNull())
			qfWarning() << "Printing an empty image";
		auto *img = new ReportItemMetaPaintImage(out, this);
		img->setSuppressPrintOut(isSuppressPrintout());
		//qfInfo() << "\t src:" << src;
		//qfInfo() << "\t processor()->images().contains(" << src << "):" << processor()->images().contains(src);
		img->aspectRatioMode = (Qt::AspectRatioMode)aspectRatio();
		/// vymysli rozmer br, do kteryho to potom reportpainter nacpe, at je to veliky jak chce
		if(designedRect.width() == 0 && designedRect.horizontalUnit == Rect::UnitMM && designedRect.height() == 0 && designedRect.verticalUnit == Rect::UnitMM) {
			/// ani jeden smer neni zadan, vezmi ozmery z obrazku
			if(im.isImage()) {
				double w = im.image.width() / (im.image.dotsPerMeterX() / 1000.);
				double h = im.image.height() / (im.image.dotsPerMeterY() / 1000.);
				if(w > 0 && w < br.width())
					br.setWidth(w);
				if(h > 0 && h < br.height())
					br.setHeight(h);
				//qfInfo() << "image bounding rect w:" << w << "h:" << h << "designed rect:" << designedRect.toString();
			}
			else if(im.isPicture()) {
				//w  =im.picture.boundingRect().width();
				//h = im.picture.boundingRect().height();
				//qfInfo() << "picture bounding rect w:" << w << "h:" << h << "designed rect:" << designedRect.toString();
			}
		}
		else if(designedRect.width() == 0 && designedRect.horizontalUnit == Rect::UnitMM) {
			/// rubber ve smeru x
			Size sz = im.size();
			//qfInfo() << "br0:" << bounding_rect.toString();
			//qfInfo() << "image size:" << sz.toString();
			{
				br = bounding_rect;
				Size br_sz = br.size();
				br_sz.setHeight(designedRect.height());
				sz.scale(br_sz, img->aspectRatioMode);
				br.setSize(sz);
				//qfInfo() << "br2:" << br.toString();
			}
		}
		else if(designedRect.height() == 0 && designedRect.verticalUnit == Rect::UnitMM) {
			/// rubber ve smeru y
			Size sz = im.size();
			br = bounding_rect;
			Size br_sz = br.size();
			br_sz.setWidth(designedRect.width());
			sz.scale(br_sz, img->aspectRatioMode);
			br.setSize(sz);
		}
		else {
			/// oba smery zadany
			Size sz = im.size();
			br = bounding_rect;
			Size br_sz = br.size();
			sz.scale(br_sz, img->aspectRatioMode);
			br.setSize(sz);
		}
		img->image = im;
		img->renderedRect = br;
		img->renderedRect.flags = designedRect.flags;
	}

	return res;
}
