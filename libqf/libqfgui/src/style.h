#ifndef QF_GUI_STYLE_H
#define QF_GUI_STYLE_H

#include "guiglobal.h"

#include <qf/core/utils.h>

#include <QObject>
#include <QSize>

class QFileInfo;

namespace qf {
namespace gui {

QFGUI_DECL_EXPORT bool isDarkTheme();

class QFGUI_DECL_EXPORT Style : public QObject
{
	Q_OBJECT
public:
	explicit Style(QObject *parent = nullptr);

	void addIconSearchPath(const QString &p);

	const QSize& defaultIconSize() const {return m_defaultIconSize;}

	QPixmap pixmap(const QString &name, const QSize &pixmap_size = QSize());
	QPixmap pixmap(const QString &name, int height);
	QIcon icon(const QString &name, const QSize &pixmap_size);
	static QIcon icon(const QString &name);

	static Style* instance();
private:
	QPixmap pixmapFromSvg(const QString &file_name, const QSize &pixmap_size = QSize()) const;
	QPixmap pixmapFromSvg(const QByteArray &svg_data, const QSize &pixmap_size = QSize()) const;
	QFileInfo findFile(const QString &path, const QString &default_extension) const;
private:
	QSize m_defaultIconSize;
	QStringList m_iconSearchPaths;
};

}}

#endif // QF_GUI_STYLE_H
