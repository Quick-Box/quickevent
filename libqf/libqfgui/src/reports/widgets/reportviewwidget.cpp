//
// Author: Frantisek Vacek <fanda.vacek@volny.cz>, (C) 2014
//
// Copyright: See COPYING file that comes with this distribution
//

#include "reportviewwidget.h"
#include "itemvalueeditorwidget.h"

#include "../processor/reportpainter.h"
#include "../processor/reportprocessor.h"
#include "../../action.h"
#include "../../dialogs/dialog.h"
#include "../../dialogs/filedialog.h"
#include "../../dialogs/messagebox.h"
#include "../../statusbar.h"
#include "../../menubar.h"
#include "../../toolbar.h"
#include "../../framework/cursoroverrider.h"
#include "../../framework/mainwindow.h"
#include "../../style.h"

#include <qf/core/utils/fileutils.h>
#include <qf/core/log.h>
#include <qf/core/string.h>

#include <QScrollBar>
#include <QMenu>
#include <QBoxLayout>
#include <QSpinBox>
#include <QBitmap>
#include <QTimer>
#include <QPrintDialog>
#include <QPrintPreviewDialog>
#include <QDesktopServices>
#include <QProcess>
#include <QUrl>
#include <QLineEdit>
#include <QDomDocument>
#include <QDir>
#include <QApplication>
#include <QTextStream>

using namespace qf::gui::reports;

//====================================================
//         ReportViewWidget::ScrollArea
//====================================================
ReportViewWidget::ScrollArea::ScrollArea(QWidget * parent)
: QScrollArea(parent)
{
	connect(verticalScrollBar(), &QScrollBar::valueChanged, this, &ReportViewWidget::ScrollArea::verticalScrollBarValueChanged);
}

bool ReportViewWidget::showReport(QWidget *parent
								, const QString &report_qml_file
								, const QVariant &single_table_data
								, const QString &window_title
								, const QString &persistent_settings_id
								, const QVariantMap &report_init_properties)
{
	QVariantMap m;
	m[QString()] = single_table_data;
	return showReport2(parent, report_qml_file, m, window_title, persistent_settings_id, report_init_properties);
}

bool ReportViewWidget::showReport2(QWidget *parent
								, const QString &report_qml_file
								, const QVariantMap &multiple_table_data
								, const QString &window_title
								, const QString &persistent_settings_id
								, const QVariantMap &report_init_properties)
{
	auto *w = new qf::gui::reports::ReportViewWidget();
	w->setWindowTitle(window_title);
	if(!persistent_settings_id.isEmpty())
		w->setPersistentSettingsId(persistent_settings_id);
	qfInfo() << "Show report:" << report_qml_file;
	w->setReport(report_qml_file, report_init_properties);
	QMapIterator<QString, QVariant> it(multiple_table_data);
	while(it.hasNext()) {
		it.next();
		w->setTableData(it.key(), it.value());
	}
	if(!parent)
		parent = qf::gui::framework::MainWindow::frameWork();
	qf::gui::dialogs::Dialog dlg(parent);
	dlg.setCentralWidget(w);
	auto report_printed = std::make_shared<bool>(false);
	connect(w, &qf::gui::reports::ReportViewWidget::reportPrinted, [report_printed](int) {
		*report_printed = true;
	});
	dlg.exec();
	return *report_printed;
}

void ReportViewWidget::ScrollArea::wheelEvent(QWheelEvent * ev)
{
#if QT_VERSION < QT_VERSION_CHECK(5, 14, 0)
	QPoint pos = ev->pos();
	auto delta = ev->delta();
#else
	QPoint pos = ev->position().toPoint();
	auto delta = ev->angleDelta().y();
#endif
	/*if(ev->orientation() == Qt::Vertical)*/ {
		if(ev->modifiers() == Qt::ControlModifier) {
			emit zoomOnWheel(delta, pos);
			ev->accept();
			return;
		}
					QScrollBar *sb = verticalScrollBar();
			if(sb) {
				if(sb->value() == sb->minimum() && delta > 0) {
					emit showPreviousPage();
					ev->accept();
					return;
				}
				if(sb->value() == sb->maximum() && delta < 0) {
					emit showNextPage();
					ev->accept();
					return;
				}
			}
			if(!sb || !sb->isVisible()) {
				/// pokud neni scroll bar, nemuzu se spolehnout na funkci verticalScrollBarValueChanged(), protoze value je pro oba smery == 0
				//qfInfo() << e->delta();
				if(delta < 0) {
					emit showNextPage();
				}
				else {
					emit showPreviousPage();
				}
				ev->accept();
				return;
			}

	}
	QScrollArea::wheelEvent(ev);
}

void ReportViewWidget::ScrollArea::keyPressEvent(QKeyEvent* ev)
{
	if(ev->modifiers() == Qt::ControlModifier) {
		static QCursor c;
		static bool c_init = false;
		if(!c_init) {
			c_init = true;
			QBitmap b1(":/qf/gui/images/zoomin_cursor_bitmap.png");
			QBitmap b2(":/qf/gui/images/zoomin_cursor_mask.png");
			c = QCursor(b1, b2, 18, 12);
		}
		setCursor(c);
	}
	QAbstractScrollArea::keyPressEvent(ev);
}

void ReportViewWidget::ScrollArea::keyReleaseEvent(QKeyEvent* ev)
{
	setCursor(QCursor());
	QWidget::keyReleaseEvent(ev);
}

void ReportViewWidget::ScrollArea::mousePressEvent(QMouseEvent* ev)
{
	if (ev->button() == Qt::LeftButton && ev->modifiers() == Qt::ControlModifier) {
		f_dragMouseStartPos = ev->pos();
		f_dragViewportStartPos = -widget()->pos();
		setCursor(QCursor(Qt::ClosedHandCursor));
		//QPoint pos = ev->pos();
		//qfInfo() << "press start pos:" << f_dragStartPos.x() << f_dragStartPos.y() << "pos:" << pos.x() << pos.y();
		//QDrag *drag = new QDrag(this);
		//QMimeData *mimeData = new QMimeData;
		//mimeData->setText(commentEdit->toPlainText());
		//drag->setMimeData(mimeData);
		//drag->setPixmap(iconPixmap);

		//Qt::DropAction dropAction = drag->exec();
		ev->accept();
		return;
	}
	QScrollArea::mousePressEvent(ev);
}

void ReportViewWidget::ScrollArea::mouseReleaseEvent(QMouseEvent* ev)
{
	setCursor(QCursor());
	QAbstractScrollArea::mouseReleaseEvent(ev);
}

void ReportViewWidget::ScrollArea::mouseMoveEvent(QMouseEvent* ev)
{
	//qfInfo() << "move buttons:" << ev->buttons() << ev->button() << "mod:" << QString::number(ev->modifiers(), 16) << (ev->button() == Qt::LeftButton) << (ev->modifiers() == Qt::ControlModifier);
	if(ev->buttons() == Qt::LeftButton && ev->modifiers() == Qt::ControlModifier) {
		QPoint pos = ev->pos();
		//qfInfo() << "start pos:" << f_dragStartPos.x() << f_dragStartPos.y() << "pos:" << pos.x() << pos.y();
		/// dej startpos na pos
		QScrollBar *hsb = horizontalScrollBar();
		QScrollBar *vsb = verticalScrollBar();
		//QSize ws = widget()->size();
		//QRect wr = widget()->geometry();
		if(hsb) {
			hsb->setValue(f_dragViewportStartPos.x() - (pos.x() - f_dragMouseStartPos.x()));
			//qfDebug() << "\t horizontal scrollbar value2:" << hsb->value() << "of" << hsb->maximum();
		}
		if(vsb) {
			vsb->setValue(f_dragViewportStartPos.y() - (pos.y() - f_dragMouseStartPos.y()));
			//qfDebug() << "\t vertical scrollbar value2:" << vsb->value() << "of" << vsb->maximum();
		}
		ev->accept();
		return;
	}
	QScrollArea::mouseMoveEvent(ev);
}

void ReportViewWidget::ScrollArea::verticalScrollBarValueChanged(int value)
{
	//qfLogFuncFrame() << "value:" << value;
	//qfInfo() << value;
	static int old_val = -1;
	QScrollBar *sb = verticalScrollBar();
	if(value == old_val) {
		if(value == sb->maximum()) {
			emit showNextPage();
		}
		else if(value == sb->minimum()) {
			emit showPreviousPage();
		}
	}
	old_val = value;
}

//====================================================
//        ReportViewWidget::PainterWidget
//====================================================
ReportViewWidget::PainterWidget::PainterWidget(QWidget *parent)
	: QWidget(parent)
{
	//screenDPMm = QPainter(this).device()->physicalDpiX() / 25.4;
	//setAutoFillBackground(true);
	//resize(210*2+20, 297*2+20);
}

ReportViewWidget* ReportViewWidget::PainterWidget::reportViewWidget()
{
	return qf::core::Utils::findParent<ReportViewWidget*>(this);
}

void ReportViewWidget::PainterWidget::paintEvent(QPaintEvent *ev)
{
	//qfDebug() << QF_FUNC_NAME;
	QWidget::paintEvent(ev);
	ReportPainter painter(this);
	painter.setMarkEditableSqlText(true);

	/// nakresli ramecek a stranku
	//painter.setBrush(Qt::yellow);
	QRect r1 = rect();
	painter.fillRect(r1, QBrush(QColor(0xCCFF99)));

	reportViewWidget()->setupPainter(&painter);
	ReportItemMetaPaintFrame *frm = reportViewWidget()->currentPage();
	if(!frm) return;
	graphics::Rect r = graphics::mm2device(frm->renderedRect, painter.device());
	painter.fillRect(r, QColor("white"));
	painter.setPen(QColor("teal"));
	painter.setBrush(QBrush());
	painter.drawRect(r);

	painter.drawMetaPaint(reportViewWidget()->currentPage());
	//painter.setPen(p);
	//QRect r(0, 0, 210, 297);
	//painter.drawText(r, Qt::AlignCenter | Qt::TextWordWrap, "<qt>Qt <b>kjutyn</b> <br>indian</qt>");
}

void ReportViewWidget::PainterWidget::mousePressEvent(QMouseEvent *e)
{
	qfLogFuncFrame();
	if(e->button() == Qt::LeftButton && e->modifiers() == Qt::ControlModifier) {
		QWidget::mousePressEvent(e); /// ignoruj posouvani reportu mysi
	}
	else {
		ReportItemMetaPaint::Point p = QPointF(e->pos());
		p = reportViewWidget()->m_painterInverseMatrix.map(p);
		p = graphics::device2mm(p, this);
		qfDebug() << Q_FUNC_INFO << ReportItem::Point(p).toString();
		reportViewWidget()->selectItem(p);
		ReportItemMetaPaint *selected_item = reportViewWidget()->selectedItem();
		if(selected_item) {
			if(e->button() == Qt::RightButton) {
				//qfInfo() << "\t item type:" << typeid(*selected_item).name();
				auto *it = dynamic_cast<ReportItemMetaPaintText*>(selected_item->firstChild());
				if(it) {
					QMenu menu(this);
					menu.setTitle(tr("Item menu"));
					QAction *act_edit = menu.addAction(tr("Editovat text"));
					/*--
					{
						QStringList edit_grants = qf::core::String(it->editGrants).splitAndTrim(',');
						bool edit_enabled = edit_grants.isEmpty();
						QFApplication *app = qobject_cast<QFApplication*>(QCoreApplication::instance());
						qfDebug() << "app:" << app << "edit grants:" << edit_grants.join(",") << "edits enabled:" << edit_enabled;
						if(app) foreach(QString grant, edit_grants)  {
							if(app->currentUserHasGrant(grant)) {
								edit_enabled = true;
								break;
							}
						}
						act_edit->setEnabled(edit_enabled);
					}
					--*/
					act_edit->setEnabled(true);
					QAction *a = menu.exec(mapToGlobal(e->pos()));
					if(a == act_edit) {
						auto *w = new ItemValueEditorWidget();
						w->setValue(it->text);
						dialogs::Dialog dlg(this);
						dlg.setCentralWidget(w);
						if(dlg.exec()) {
							QVariant val = w->value();
							it->text = val.toString();
							/// nevim proc, ale \n mi to tiskne, jako zvonecek :)
							it->text.replace('\n', QChar::LineSeparator);
							/// roztahni text na ohranicujici ramecek, aby se tam delsi text vesel
							it->setRenderedRectRect(selected_item->renderedRect);
							update();
							QString sql_id = it->sqlId;
							if(!sql_id.isEmpty()) {
								emit sqlValueEdited(sql_id, val);
							}
						}
					}
				}
			}
		}
	}
}

//====================================================
//                                 ReportViewWidget
//====================================================
ReportViewWidget::ReportViewWidget(QWidget *parent)
	: Super(parent), m_scrollArea(nullptr), edCurrentPage(nullptr), m_statusBar(nullptr)
{
	qfLogFuncFrame() << this << "parent:" << parent;
	m_reportProcessor = nullptr;
	m_whenRenderingSetCurrentPageTo = -1;
	/*--
	f_uiBuilder = new QFUiBuilder(this, ":/libqfgui/qfreportviewwidget.ui.xml");
	f_actionList += f_uiBuilder->actions();
	--*/
	//--action("file.export.email")->setVisible(false);
	//action("report.edit")->setVisible(false);

	m_currentPageNo = -1;
	m_selectedItem = nullptr;

	setPersistentSettingsId("reportViewWidget");

	m_scrollArea = new ScrollArea(nullptr);
	/// zajimavy, odkomentuju tenhle radek a nemuzu nastavit pozadi zadnyho widgetu na scrollArea.
	//f_scrollArea->setBackgroundRole(QPalette::Dark);
	m_painterWidget = new PainterWidget(m_scrollArea);
	connect(m_painterWidget, &PainterWidget::sqlValueEdited, this, &ReportViewWidget::sqlValueEdited, Qt::QueuedConnection);
	connect(m_scrollArea, &ScrollArea::zoomOnWheel, this, &ReportViewWidget::zoomOnWheel, Qt::QueuedConnection);
	m_scrollArea->setWidget(m_painterWidget);
	QBoxLayout *ly = new QVBoxLayout(this);
	ly->setSpacing(0);
	ly->setContentsMargins(0, 0, 0, 0);
	ly->addWidget(m_scrollArea);
	ly->addWidget(statusBar());

	connect(m_scrollArea, &ScrollArea::showNextPage, this, &ReportViewWidget::scrollToNextPage);
	connect(m_scrollArea, &ScrollArea::showPreviousPage, this, &ReportViewWidget::scrollToPrevPage);
}

ReportViewWidget::~ReportViewWidget()
{
	qfLogFuncFrame() << this;
}

ReportProcessor * ReportViewWidget::reportProcessor()
{
	if(m_reportProcessor == nullptr) {
		setReportProcessor(new ReportProcessor(m_painterWidget, this));
	}
	return m_reportProcessor;
}

void ReportViewWidget::setReportProcessor(ReportProcessor * proc)
{
	m_reportProcessor = proc;
	connect(m_reportProcessor, &ReportProcessor::pageProcessed, this, &ReportViewWidget::onPageProcessed);
}

qf::gui::StatusBar *ReportViewWidget::statusBar()
{
	if(!m_statusBar) {
		m_statusBar = new gui::StatusBar(nullptr);
		zoomStatusSpinBox = new QSpinBox();
		zoomStatusSpinBox->setSingleStep(10);
		zoomStatusSpinBox->setMinimum(10);
		zoomStatusSpinBox->setMaximum(1000000);
		zoomStatusSpinBox->setPrefix("zoom: ");
		zoomStatusSpinBox->setSuffix("%");
		zoomStatusSpinBox->setAlignment(Qt::AlignRight);
		m_statusBar->addWidget(zoomStatusSpinBox);
		connect(zoomStatusSpinBox, &QSpinBox::valueChanged, this, &ReportViewWidget::setScaleProc);
	}
	return m_statusBar;
}

void ReportViewWidget::view_zoomIn(const QPoint &center_pos)
{
	qfLogFuncFrame() << "center_pos:" << center_pos.x() << center_pos.y();
	const QRect visible_rect(-m_scrollArea->widget()->pos(), m_scrollArea->viewport()->size());
	//QSizeF old_report_size = f_scrollArea->widget()->size();
	QPointF old_abs_center_pos = visible_rect.topLeft() + center_pos;
	qfDebug() << "visible rect:" << gui::graphics::Rect(visible_rect).toString();
	QScrollBar *hsb = m_scrollArea->horizontalScrollBar();
	QScrollBar *vsb = m_scrollArea->verticalScrollBar();

	qreal old_scale = scale(), new_scale;
	//QPointF old_report_pos = QPointF(abs_center_pos) / old_scale;
	//if(old_scale > 1) new_scale = old_scale + 1;
	//else if(old_scale > 1) new_scale = old_scale + 1;
	//else
	new_scale = old_scale * 1.33;
	setScale(new_scale);
	//qfInfo() << "new scale:" << new_scale << "old scale:" << old_scale;
	if(!center_pos.isNull()) {
		QPointF new_abs_center_pos = old_abs_center_pos * (new_scale / old_scale);
		if(hsb) {
			hsb->setValue(new_abs_center_pos.x() - center_pos.x());
			qfDebug() << "\t horizontal scrollbar value2:" << hsb->value() << "of" << hsb->maximum();
		}
		if(vsb) {
			vsb->setValue(new_abs_center_pos.y() - center_pos.y());
			qfDebug() << "\t vertical scrollbar value2:" << vsb->value() << "of" << vsb->maximum();
		}
	}
}

void ReportViewWidget::view_zoomOut(const QPoint &center_pos)
{
	qfLogFuncFrame() << "center_pos:" << center_pos.x() << center_pos.y();
	const QRect visible_rect(-m_scrollArea->widget()->pos(), m_scrollArea->viewport()->size());
	QPointF old_abs_center_pos = visible_rect.topLeft() + center_pos;
	qfDebug() << "visible rect:" << gui::graphics::Rect(visible_rect).toString();
	QScrollBar *hsb = m_scrollArea->horizontalScrollBar();
	QScrollBar *vsb = m_scrollArea->verticalScrollBar();

	qreal old_scale = scale(), new_scale;
	new_scale = old_scale / 1.33;
	setScale(new_scale);

	if(!center_pos.isNull()) {
		QPointF new_abs_center_pos = old_abs_center_pos * (new_scale / old_scale);
		if(hsb) {
			hsb->setValue(new_abs_center_pos.x() - center_pos.x());
			qfDebug() << "\t horizontal scrollbar value2:" << hsb->value() << "of" << hsb->maximum();
		}
		if(vsb) {
			vsb->setValue(new_abs_center_pos.y() - center_pos.y());
			qfDebug() << "\t vertical scrollbar value2:" << vsb->value() << "of" << vsb->maximum();
		}
	}
}

void ReportViewWidget::zoomOnWheel(int delta, const QPoint &center_pos)
{
	if(delta > 0)
		view_zoomIn(center_pos);
	else if(delta < 0)
		view_zoomOut(center_pos);
}

void ReportViewWidget::view_zoomToFitWidth()
{
	qfDebug() << QF_FUNC_NAME;
	ReportItemMetaPaintFrame *frm = currentPage();
	if(!frm) return;
	auto r = frm->renderedRect;
	double report_px = (r.width() + 2 * PageBorder) * logicalDpiX() / 25.4;
	double widget_px = m_scrollArea->width();
	//QScrollBar *sb = f_scrollArea->verticalScrollBar();
	//if(sb) widget_px -= sb->width();
	double sc = widget_px / report_px * 0.98;
	setScale(sc);
}

void ReportViewWidget::view_zoomToFitHeight()
{
	ReportItemMetaPaintFrame *frm = currentPage();
	if(!frm) return;
	auto r = frm->renderedRect;
	double report_px = (r.height() + 2 * PageBorder) * m_painterWidget->logicalDpiY() / 25.4;
	double widget_px = m_scrollArea->height();
	double sc = widget_px / report_px * 0.98;
	setScale(sc);
}

void ReportViewWidget::setScale(qreal _scale)
{
	qfLogFuncFrame() << currentPageNo();
	ReportItemMetaPaintFrame *frm = currentPage();
	if(!frm)
		return;

	m_scale = _scale;
	setupPainterWidgetSize();
	m_painterWidget->update();
	refreshWidget();
}

void ReportViewWidget::settleDownInDialog(qf::gui::dialogs::Dialog *dlg)
{
	qfLogFuncFrame();
	qf::gui::Action *act_file = dlg->menuBar()->actionForPath("file");
	act_file->setText(tr("&File"));
	act_file->addActionInto(action("file.print"));
	act_file->addActionInto(action("file.printPreview"));
	act_file->addSeparatorInto();
	act_file->addActionInto(action("file.export.pdf"));
	act_file->addActionInto(action("file.export.html"));

	qf::gui::Action *act_view = dlg->menuBar()->actionForPath("view");
	act_view->setText(tr("&View"));
	act_view->addActionInto(action("view.firstPage"));
	act_view->addActionInto(action("view.prevPage"));
	act_view->addActionInto(action("view.nextPage"));
	act_view->addActionInto(action("view.lastPage"));
	act_view->addActionInto(action("view.zoomIn"));
	act_view->addActionInto(action("view.zoomOut"));
	act_view->addActionInto(action("view.zoomFitWidth"));
	act_view->addActionInto(action("view.zoomFitHeight"));

	qf::gui::ToolBar *tool_bar = dlg->toolBar("main", true);
	tool_bar->addAction(action("file.print"));
	tool_bar->addAction(action("file.export.pdf"));
	tool_bar->addSeparator();
	tool_bar->addAction(action("view.firstPage"));
	tool_bar->addAction(action("view.prevPage"));
	m_edCurrentPage = new QLineEdit;
	m_edCurrentPage->setSizePolicy(QSizePolicy::Maximum, QSizePolicy::Preferred);
	m_edCurrentPage->setAlignment(Qt::AlignRight);
	tool_bar->addWidget(m_edCurrentPage);
	tool_bar->addAction(action("view.nextPage"));
	tool_bar->addAction(action("view.lastPage"));
	tool_bar->addAction(action("view.zoomIn"));
	tool_bar->addAction(action("view.zoomOut"));
	tool_bar->addAction(action("view.zoomFitWidth"));
	tool_bar->addAction(action("view.zoomFitHeight"));
}

qf::gui::framework::DialogWidget::ActionMap ReportViewWidget::createActions()
{
	ActionMap ret;
	{
		qf::gui::Action *a;
		a = new qf::gui::Action(tr("First page"), this);
		a->setIcon(qf::gui::Style::icon("skip-back"));
		ret[QStringLiteral("view.firstPage")] = a;
		connect(a, &QAction::triggered, this, &ReportViewWidget::view_firstPage);
	}
	{
		qf::gui::Action *a;
		a = new qf::gui::Action(tr("Prev page"), this);
		a->setIcon(qf::gui::Style::icon("step-back"));
		ret[QStringLiteral("view.prevPage")] = a;
		connect(a, &QAction::triggered, this, [this]() { view_prevPage(); });
	}
	{
		qf::gui::Action *a;
		a = new qf::gui::Action(tr("Next page"), this);
		a->setIcon(qf::gui::Style::icon("step-forward"));
		ret[QStringLiteral("view.nextPage")] = a;
		connect(a, &QAction::triggered, this, [this]() { view_nextPage(); });
	}
	{
		qf::gui::Action *a;
		a = new qf::gui::Action(tr("Last page"), this);
		a->setIcon(qf::gui::Style::icon("skip-forward"));
		ret[QStringLiteral("view.lastPage")] = a;
		connect(a, &QAction::triggered, this, &ReportViewWidget::view_lastPage);
	}
	{
		qf::gui::Action *a;
		a = new qf::gui::Action(tr("Zoom in"), this);
		a->setIcon(qf::gui::Style::icon("zoom-in"));
		ret[QStringLiteral("view.zoomIn")] = a;
		connect(a, &QAction::triggered, this, [this]() { view_zoomIn(); });
	}
	{
		qf::gui::Action *a;
		a = new qf::gui::Action(tr("Zoom out"), this);
		a->setIcon(qf::gui::Style::icon("zoom-out"));
		ret[QStringLiteral("view.zoomOut")] = a;
		connect(a, &QAction::triggered, this, [this]() { view_zoomOut(); });
	}
	{
		qf::gui::Action *a;
		a = new qf::gui::Action(tr("Zoom to fit width"), this);
		a->setIcon(qf::gui::Style::icon("zoom-fitwidth"));
		ret[QStringLiteral("view.zoomFitWidth")] = a;
		connect(a, &QAction::triggered, this, &ReportViewWidget::view_zoomToFitWidth);
	}
	{
		qf::gui::Action *a;
		a = new qf::gui::Action(tr("Zoom to fit height"), this);
		a->setIcon(qf::gui::Style::icon("zoom-fitheight"));
		ret[QStringLiteral("view.zoomFitHeight")] = a;
		connect(a, &QAction::triggered, this, &ReportViewWidget::view_zoomToFitHeight);
	}
	{
		qf::gui::Action *a;
		a = new qf::gui::Action(tr("&Print"), this);
		a->setIcon(qf::gui::Style::icon("printer"));
		//a->setTooltip(tr("Tisk"));
		ret[QStringLiteral("file.print")] = a;
		connect(a, &QAction::triggered, this, &ReportViewWidget::file_print);
	}
	{
		qf::gui::Action *a;
		a = new qf::gui::Action(tr("Print pre&view"), this);
		a->setIcon(qf::gui::Style::icon("print-preview"));
		ret[QStringLiteral("file.printPreview")] = a;
		connect(a, &QAction::triggered, this, &ReportViewWidget::file_printPreview);
	}
	{
		qf::gui::Action *a;
		a = new qf::gui::Action(tr("Export PD&F"), this);
		a->setIcon(qf::gui::Style::icon("acrobat"));
		a->setToolTip(tr("Export in the Adobe Acrobat PDF format"));
		ret[QStringLiteral("file.export.pdf")] = a;
		connect(a, &QAction::triggered, this, &ReportViewWidget::file_export_pdf);
	}
	{
		qf::gui::Action *a;
		a = new qf::gui::Action(tr("Export &HTML"), this);
		a->setIcon(qf::gui::Style::icon("network"));
		a->setToolTip(tr("Export data in HTML"));
		ret[QStringLiteral("file.export.html")] = a;
		connect(a, &QAction::triggered, this, &ReportViewWidget::file_export_html);
	}
	return ret;
}

void ReportViewWidget::setupPainterWidgetSize()
{
	qfLogFuncFrame();
	ReportItemMetaPaintFrame *frm = currentPage();
	if(!frm)
		return;
	gui::graphics::Rect r1 = frm->renderedRect.adjusted(-PageBorder, -PageBorder, PageBorder, PageBorder);
	gui::graphics::Rect r2 = gui::graphics::mm2device(r1, m_painterWidget);
	//qfDebug() << "\tframe rect:" << r.toString();
	QSizeF s = r2.size();
	s *= scale();
	//painterScale = QSizeF(s.width() / r1.width(), s.height() / r1.height());
	m_painterWidget->resize(s.toSize());
}

void ReportViewWidget::setupPainter(ReportPainter *p)
{
	QF_ASSERT(p != nullptr, "painter is NULL", return);
	//p->currentPage = currentPageNo();
	p->pageCount = pageCount();
	p->setSelectedItem(m_selectedItem);
	p->scale(scale(), scale());
	p->translate(gui::graphics::mm2device(gui::graphics::Point(PageBorder, PageBorder), p->device()));
#if QT_VERSION < QT_VERSION_CHECK(5, 13, 0)
	m_painterInverseMatrix = p->matrix().inverted();
#else
	m_painterInverseMatrix = p->worldTransform().inverted();
#endif
}

void ReportViewWidget::setReport(const QString &file_name, const QVariantMap &report_init_properties)
{
	qfLogFuncFrame() << "file_name:" << file_name;
	//qfDebug() << "\tdata:" << fData.toString();
	reportProcessor()->setReport(file_name, report_init_properties);
	//out.dump();
}

void ReportViewWidget::onPageProcessed()
{
	qfLogFuncFrame();
	if(m_whenRenderingSetCurrentPageTo >= 0) {
		if(pageCount() - 1 == m_whenRenderingSetCurrentPageTo) {
			setCurrentPageNo(m_whenRenderingSetCurrentPageTo);
			m_whenRenderingSetCurrentPageTo = -1;
		}
	}
	else {
		if(pageCount() == 1)
			setCurrentPageNo(0);
	}
	refreshWidget();
	//setCurrentPageNo(0);
	QTimer::singleShot(10, reportProcessor(), &ReportProcessor::processSinglePage); /// 10 je kompromis mezi rychlosti prekladu a sviznosti GUI
}

ReportItemMetaPaintReport* ReportViewWidget::document(bool throw_exc)
{
	ReportItemMetaPaintReport *doc = reportProcessor()->processorOutput();
	if(!doc && throw_exc)
		QF_EXCEPTION("document is NULL");
	return doc;
}

void ReportViewWidget::setTableData(const QString &key, const qf::core::utils::TreeTable &table_data)
{
	qfLogFuncFrame();
	reportProcessor()->setTableData(key, table_data);
}

void ReportViewWidget::setTableData(const QString &key, const QVariant &table_data)
{
	reportProcessor()->setTableData(key, table_data);
}

int ReportViewWidget::pageCount()
{
	//qfLogFuncFrame();
	int ret = 0;
	if(document(!qf::core::Exception::Throw)) {
		ret = document()->childrenCount();
	}
	return ret;
}

void ReportViewWidget::setCurrentPageNo(int pg_no)
{
	if(pg_no >= pageCount() || pg_no < 0)
		pg_no = 0;
	m_currentPageNo = pg_no;
	setupPainterWidgetSize();
	m_painterWidget->update();
	refreshWidget();
}

ReportItemMetaPaintFrame* ReportViewWidget::getPage(int n)
{
	//qfDebug() << QF_FUNC_NAME << currentPageNo();
	if(!document(!qf::core::Exception::Throw))
		return nullptr;
	if(n < 0 || n >= document()->childrenCount())
		return nullptr;
	ReportItemMetaPaint *it = document()->child(n);
	auto *frm = dynamic_cast<ReportItemMetaPaintFrame*>(it);
	//qfDebug() << "\treturn:" << frm;
	return frm;
}

ReportItemMetaPaintFrame* ReportViewWidget::currentPage()
{
	ReportItemMetaPaintFrame *frm = getPage(currentPageNo());
	//qfDebug() << QF_FUNC_NAME << currentPageNo();
	if(!frm)
		return nullptr;
	return frm;
}
/*--
void ReportViewWidget::selectElement_helper(ReportItemMetaPaint *it, const QFDomElement &el)
{
	QDomElement el1 = it->reportItem()->element;
	if(el1 == el) {
		//qfInfo() << "BINGO";
		f_selectedItem = it;
		qfDebug() << "\t EMIT:" << el1.tagName();
		emit elementSelected(el1);
	}
	else {
		foreach(QFTreeItemBase *_it, it->children()) {
			ReportItemMetaPaint *it1 = static_cast<ReportItemMetaPaint*>(_it);
			selectElement_helper(it1, el);
		}
	}
}

void ReportViewWidget::selectElement(const QFDomElement &el)
{
	qfLogFuncFrame() << el.tagName();
	if(!el) return;
	//qfInfo() << __LINE__;
	#if 0
	kdyz se tohle odkomentuje, tak to pada
	if(f_selectedItem && f_selectedItem->reportItem()->element == el) {
		qfDebug() << "\t allready selected";
		return;
	}
	#endif
	f_selectedItem = nullptr;
	ReportItemMetaPaintFrame *frm = currentPage();
	if(!frm) return;
	selectElement_helper(frm, el);
	f_painterWidget->update();
}

static QString is_fake_element(const QFDomElement &_el)
{
	qfLogFuncFrame();
	QString fake_path;
	QFDomElement el = _el;
	while(!!el) {
		//qfError() << el.toString();
		fake_path = el.attribute("__fakePath");
		qfDebug() << "\t fake path of" << el.tagName() << "=" << fake_path;
		if(!fake_path.isEmpty()) break;
		else if(el.hasAttribute("__fake")) {
			fake_path = "-";
			break;
		}
		el = el.parentNode().toElement();
	}
	qfDebug() << "\t return:" << fake_path;
	return fake_path;
}
--*/
ReportItemMetaPaint* ReportViewWidget::selectItem_helper(ReportItemMetaPaint *it, const QPointF &p)
{
	ReportItemMetaPaint *ret = nullptr;
	if(it && it->isPointInside(p)) {
		//qfLogFuncFrame() << "point inside:" << it->reportItem() << it->renderedRect.toString();
		ret = it;
		//qfInfo() << it->dump();
		/// traverse items in reverse order to select top level items in stacked layout
		auto chlst = it->children();
		for(int i=chlst.count()-1; i>=0; i--) {
			auto *it1 = static_cast<ReportItemMetaPaint*>(chlst[i]);
			ReportItemMetaPaint *child_sel_it = selectItem_helper(it1, p);
			if(child_sel_it) {
				if(ret == it)
					ret = child_sel_it;
				else {
					// if more children have item under cursor,
					// then select one with smaller area
					ReportItem::Rect r1 = ret->renderedRect;
					ReportItem::Rect r2 = child_sel_it->renderedRect;
					if(r1.area() > r2.area()) {
						ret = child_sel_it;
					}
				}
			}
		}
	}
	return ret;
}

void ReportViewWidget::selectItem(const QPointF &p)
{
	qfLogFuncFrame();
	ReportItemMetaPaintFrame *frm = currentPage();
	ReportItemMetaPaint *old_selected_item = m_selectedItem;
	//QFDomElement old_el = fSelectedElement;
	m_selectedItem = selectItem_helper(frm, p);
	if(m_selectedItem != old_selected_item) {
		/// odznac puvodni selekci
		m_painterWidget->update();
	}
}

void ReportViewWidget::setVisible(bool visible)
{
	qfLogFuncFrame() << "visible:" << visible;
	Super::setVisible(visible);
	if(visible)
		processReport();
}

void ReportViewWidget::processReport()
{
	qfLogFuncFrame();
	if(!reportProcessor()->processorOutput()) {
		reportProcessor()->process(ReportProcessor::FirstPage);
	}
	setCurrentPageNo(0);
	setScale(1);
}
/*
void ReportViewWidget::render()
{
	qfLogFuncFrame();
	m_whenRenderingSetCurrentPageTo = currentPageNo();
	reportProcessor()->reset();
	if(!reportProcessor()->processorOutput()) {
		//qfInfo() << "process report";
		reportProcessor()->process(ReportProcessor::FirstPage);
	}
	//qfInfo() << "setCurrentPageNo:" << cur_page_no;
	//setCurrentPageNo(cur_page_no);
}
*/
void ReportViewWidget::refreshWidget()
{
	statusBar();
	if(m_edCurrentPage)
		m_edCurrentPage->setText(QString::number(currentPageNo()+1) + "/" + QString::number(pageCount()));
	refreshActions();
	zoomStatusSpinBox->setValue((int)(scale() * 100));
	//statusBar()->setText("zoom: " + QString::number((int)(scale() * 100)) + "%");
	QApplication::processEvents();
}

void ReportViewWidget::refreshActions()
{
	int pgno = currentPageNo();
	int pgcnt = pageCount();
	qfLogFuncFrame() << pgno << "of" << pgcnt;
	action("view.firstPage")->setEnabled(pgno > 0 && pgcnt > 0);
	action("view.prevPage")->setEnabled(pgno > 0 && pgcnt > 0);
	action("view.nextPage")->setEnabled(pgno < pgcnt - 1);
	action("view.lastPage")->setEnabled(pgno < pgcnt - 1);
}

void ReportViewWidget::view_nextPage(PageScrollPosition scroll_pos)
{
	qfDebug() << QF_FUNC_NAME;
	if(currentPageNo() < pageCount() - 1) {
		setCurrentPageNo(currentPageNo() + 1);
		if(scroll_pos == ScrollToPageTop) m_scrollArea->verticalScrollBar()->setValue(m_scrollArea->verticalScrollBar()->minimum());
		else if(scroll_pos == ScrollToPageEnd) m_scrollArea->verticalScrollBar()->setValue(m_scrollArea->verticalScrollBar()->maximum());
	}
}

void ReportViewWidget::view_prevPage(PageScrollPosition scroll_pos)
{
	qfDebug() << QF_FUNC_NAME;
	if(currentPageNo() > 0) {
		setCurrentPageNo(currentPageNo() - 1);
		if(scroll_pos == ScrollToPageTop) m_scrollArea->verticalScrollBar()->setValue(m_scrollArea->verticalScrollBar()->minimum());
		else if(scroll_pos == ScrollToPageEnd) m_scrollArea->verticalScrollBar()->setValue(m_scrollArea->verticalScrollBar()->maximum());
	}
}

void ReportViewWidget::view_firstPage()
{
	qfDebug() << QF_FUNC_NAME;
	int pgno = 0;
	if(pgno < pageCount()) setCurrentPageNo(pgno);
}

void ReportViewWidget::view_lastPage()
{
	qfDebug() << QF_FUNC_NAME;
	int pgno = pageCount() - 1;
	if(pgno >= 0) setCurrentPageNo(pgno);
}

void ReportViewWidget::edCurrentPageEdited()
{
	qfDebug() << QF_FUNC_NAME;
	QStringList sl = edCurrentPage->text().split("/");
	if(sl.count() > 0) {
		int pg = sl[0].toInt() - 1;
		setCurrentPageNo(pg);
	}
}

void ReportViewWidget::print(QPrinter *printer)
{
	if(printer)
		print(*printer);
}

void ReportViewWidget::print(QPrinter &printer, const QVariantMap &options)
{
	qfLogFuncFrame();

	framework::CursorOverrider cov(Qt::WaitCursor);

	ReportPainter painter(&printer);

	typedef ReportItem::Rect Rect;

	int pg_no = options.value("fromPage", 1).toInt() - 1;
	int to_page = options.value("toPage", pageCount()).toInt();
	qfDebug() << "pg_no:" << pg_no << "to_page:" << to_page;
	ReportItemMetaPaintFrame *frm = getPage(pg_no);
	if(frm) {
		Rect r = frm->renderedRect;
		bool landscape = r.width() > r.height();
		if(landscape)
			printer.setPageOrientation(QPageLayout::Landscape);
		//Rect printer_pg_rect = QRectF(printer.pageRect());
		//qfWarning() << "\tprinter page rect:" << printer_pg_rect.toString();
		//qfWarning() << "\tresolution:" << printer.resolution() << Size(printer_pg_rect.size()/printer.resolution()).toString(); /// resolution je v DPI
		//qreal magnify = printer_pg_rect.width() / r.width();
		//painter.scale(magnify, magnify);
		painter.pageCount = pageCount();
		while(frm) {
			//painter.currentPage = pg_no;
			painter.drawMetaPaint(frm);
			pg_no++;
			frm = getPage(pg_no);
			if(!frm)
				break;
			if(pg_no >= to_page)
				break;
			printer.newPage();
		}
	}
	emit reportPrinted(printer.outputFormat());
}

void ReportViewWidget::print()
{
	qfLogFuncFrame();

	QPrinter printer;
	printer.setOutputFormat(QPrinter::NativeFormat);
	//printer.setOutputFileName(fn);
	printer.setFullPage(true);
	printer.setPageSize(QPageSize{QPageSize::A4});
	printer.setPageOrientation(document()->orientation);

	QPrintDialog dlg(&printer, this);
	if(dlg.exec() != QDialog::Accepted)
		return;

	qfDebug() << "options:" << dlg.options();
	QVariantMap opts;
	if(dlg.testOption(QAbstractPrintDialog::PrintPageRange)) { /// tohle je nastaveny vzdycky :(
		int from_page = dlg.fromPage();
		int to_page = dlg.toPage();
		qfDebug() << "fromPage:" << dlg.fromPage() << "toPage:" << dlg.toPage();
		if(from_page > 0)
			opts["fromPage"] = dlg.fromPage();
		if(to_page > 0)
			opts["toPage"] = dlg.toPage();
	}

	print(printer, opts);
}

void ReportViewWidget::exportPdf(const QString &file_name)
{
	qfLogFuncFrame();
	QString fn = file_name;
	if(fn.isEmpty())
		QF_EXCEPTION(tr("empty file name"));
	auto ext = QStringLiteral(".pdf");
	if(!fn.endsWith(ext, Qt::CaseInsensitive)) {
		fn += ext;
	}
	QPrinter printer;
	printer.setOutputFormat(QPrinter::PdfFormat);
	printer.setOutputFileName(fn);
	printer.setFullPage(true);
	printer.setPageSize(QPageSize{QPageSize::A4});
	printer.setPageOrientation(document()->orientation);
	/// zatim bez dialogu
	//QPrintDialog printDialog(&printer, parentWidget());
	//if(printDialog.exec() != QDialog::Accepted) return;

	print(printer);
}

namespace {
QString addHtmlEnvelope(const QString &html_body_content)
{
	QString eoln = QStringLiteral("\n");
	QString ret;
	ret += "<html>" + eoln;
	ret += "<head>" + eoln;
	ret += R"(<meta http-equiv="content-type" content="text/html; charset=utf-8"/>)" + eoln;
	ret += "<title>Data</title>" + eoln;
	ret += "<style type=\"text/css\">"
			"th {background-color:khaki; font-weight:bold;}"
			"table {border-collapse: collapse;}"
			"</style>" + eoln;
	ret += "</head>" + eoln;
	ret += "<body>" + eoln;
	ret += html_body_content;
	ret += "</body>" + eoln;
	ret += "</html>" + eoln;
	return ret;
}

}
QString ReportViewWidget::exportHtml()
{
	qfLogFuncFrame();
	QDomDocument doc;
	doc.setContent(addHtmlEnvelope(QString()));
	QDomElement el_body = doc.documentElement().firstChildElement("body");
	framework::CursorOverrider cov(Qt::WaitCursor);
	//QApplication::setOverrideCursor(QCursor(Qt::WaitCursor));
	reportProcessor()->processHtml(el_body);
	//QApplication::restoreOverrideCursor();
	return doc.toString(2);
}

void ReportViewWidget::file_export_pdf(bool open)
{
	qfDebug() << QF_FUNC_NAME;
	//reportProcessor()->dump();
	QString fn;
	QString ext = ".pdf";
	if(open)
		fn = QDir::tempPath() + "/report.pdf";
	else
		fn = dialogs::FileDialog::getSaveFileName (this, tr("Save as PDF"), QString(), '*' + ext);
	if(fn.isEmpty())
		return;
	if(!fn.endsWith(ext, Qt::CaseInsensitive))
		fn += ext;
	exportPdf(fn);
	if(open) {
		QDesktopServices::openUrl(QUrl::fromLocalFile(fn));
	}
}

void ReportViewWidget::file_export_html()
{
	qfLogFuncFrame();
	QString fn = "report.html";
	fn = dialogs::FileDialog::getSaveFileName (this, tr("Save as HTML"), fn, "*.html");
	if(!fn.isEmpty()) {
		QString s = exportHtml();
		QFile f(fn);
		if(!f.open(QFile::WriteOnly)) {
			dialogs::MessageBox::showError(this, tr("Cannot open '%1' for write.").arg(f.fileName()));
			return;
		}
		QTextStream out(&f);
#if QT_VERSION_MAJOR >= 6
		out.setEncoding(QStringConverter::encodingForName("UTF-8").value());
#else
		out.setCodec("UTF-8");
#endif

		out << s;
	}
}

void ReportViewWidget::file_print()
{
	qfDebug() << QF_FUNC_NAME;
	print();
}

void ReportViewWidget::file_printPreview()
{
	qfLogFuncFrame();
	QPrinter printer(QPrinter::ScreenResolution);	/// SVG to tiskne na preview moc velky, pokud je QPrinter printer(QPrinter::HighResolution), je to potreba opravit
	QPrintPreviewDialog preview(&printer, this);
	connect(&preview, &QPrintPreviewDialog::paintRequested, this, [this]() { print(); });
	preview.exec();
}


