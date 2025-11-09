#include "desktoputils.h"

#include <QApplication>
#include <QScreen>



namespace qf::gui::internal {

QRect DesktopUtils::moveRectToVisibleDesktopScreen(const QRect &rect)
{
	auto *scr = QApplication::screenAt(rect.topLeft());
	if (!scr) {
		scr = QApplication::primaryScreen();
	}
	if (!scr) {
		return rect;
	}
	auto ret = rect;
	auto screen_rect = scr->geometry();
	if (!screen_rect.contains(rect.topLeft())) {
		ret.moveTopLeft(screen_rect.topLeft());
	}
	if (ret.size().width() > screen_rect.size().width()) {
		ret.setWidth(screen_rect.width());
	}
	if (ret.size().height() > screen_rect.size().height()) {
		ret.setHeight(screen_rect.height());
	}
	return ret;
}

} // namespace qf::gui::internal



