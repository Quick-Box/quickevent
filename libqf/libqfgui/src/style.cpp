#include "style.h"

#include <qf/core/log.h>

#include <QCoreApplication>
#include <QIcon>
#include <QSvgRenderer>
#include <QFile>
#include <QPainter>
#include <QFileInfo>
#include <QApplication>
#include <QPalette>
#include <QStyleHints>

namespace qf::gui {

bool isDarkTheme()
{
	// MUST be called after QApplication instance is created
	return qApp->styleHints()->colorScheme() == Qt::ColorScheme::Dark;
}

Style::Style(QObject *parent)
	: QObject(parent)
{
	m_defaultIconSize = QSize(32, 32);
	//setDefaultIconSize(QSize(16, 16));
}

void Style::addIconSearchPath(const QString &p)
{
	m_iconSearchPaths << p;
}

QPixmap Style::pixmapFromSvg(const QString &file_name, const QSize &pixmap_size) const
{
	qfLogFuncFrame() << file_name << pixmap_size;
	QPixmap ret;
	QSize px_sz = pixmap_size;
	if(px_sz.isEmpty())
		px_sz = defaultIconSize();
	QFile f(file_name);
	qfDebug() << "opening:" << file_name << "...";
	if(f.open(QFile::ReadOnly)) {
		qfDebug() << "OK";
		QByteArray ba = f.readAll();
		if (isDarkTheme()) {
			ba.replace("\"currentColor\"", "\"lightgray\"");
		}
		QSvgRenderer rnd(ba);
		ret = QPixmap(px_sz);
		ret.fill(Qt::transparent);
		QPainter painter(&ret);
		QRect r(QPoint(0, 0), px_sz);
		QSize svg_sz = rnd.defaultSize();
		if(svg_sz.height() < svg_sz.width()) {
			int new_h = px_sz.width() * svg_sz.height() / svg_sz.width();
			int pos = (px_sz.width() - new_h) / 2;
			r = QRect(0, pos, px_sz.width(), new_h - pos);
		}
		else if(svg_sz.width() < svg_sz.height()) {
			int new_w = px_sz.height() * svg_sz.width() / svg_sz.height();
			int pos = (px_sz.width() - new_w) / 2;
			r = QRect(pos, 0, new_w - pos, px_sz.height());
		}
		rnd.render(&painter, r);
	}
	return ret;
}

QPixmap Style::pixmap(const QString &name, const QSize &pixmap_size)
{
	qfLogFuncFrame() << name << pixmap_size;
	QSize sz = pixmap_size;
	if(sz.isEmpty())
		sz = defaultIconSize();
	if(auto fi = findFile(name, ".svg"); fi.exists()) {
		return pixmapFromSvg(fi.absoluteFilePath(), sz);
	}
	if(auto fi = findFile(name, ".png"); fi.exists()) {
		QPixmap ret(fi.absoluteFilePath());
		if(!ret.isNull())
			ret = ret.scaled(sz, Qt::KeepAspectRatio, Qt::SmoothTransformation);
		return ret;
	}
	qfWarning() << "Cannot find file for icon:" << name;
	return {};
}

QPixmap Style::pixmap(const QString &name, int height)
{
	return pixmap(name, QSize(height, height));
}

QIcon Style::icon(const QString &name, const QSize &pixmap_size)
{
	QIcon ret;
	ret.addPixmap(pixmap(name, pixmap_size));
	return ret;
}

QIcon Style::icon(const QString &name)
{
	return instance()->icon(name, QSize());
}

Style *Style::instance()
{
	QCoreApplication *app = QCoreApplication::instance();
	if(app) {
		auto *style = app->findChild<Style*>({}, Qt::FindDirectChildrenOnly);
		if(!style) {
			style = new Style(app);
		}
		return style;
	}
	qfError() << "qf::qmlwidget::Style::instance is available only when QCoreApplication instance exists.";
	return nullptr;
}

QFileInfo Style::findFile(const QString &path, const QString &default_extension) const
{
	QString fn = path;
	if(!fn.endsWith(default_extension)) {
		fn += default_extension;
	}
	QFileInfo fi(fn);
	if(fi.isAbsolute()) {
		if(fi.isReadable()) {
			return fi;
		}
	}
	else {
		for(const auto &prefix : m_iconSearchPaths) {
			auto fn2 = prefix + '/' + fn;
			QFileInfo fi2(fn2);
//			qfInfo() << "checking:" << fn2;
			if(fi2.isReadable()) {
//				qfInfo() << "\t\tHHIITT" << fn2;
				return fi2;
			}
		}
	}
	return {};
}

}
