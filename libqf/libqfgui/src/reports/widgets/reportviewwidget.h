//
// Author: Frantisek Vacek <fanda.vacek@volny.cz>, (C) 2014
//
// Copyright: See COPYING file that comes with this distribution
//

#ifndef QF_GUI_REPORTS_REPORTVIEWWIDGET_H
#define QF_GUI_REPORTS_REPORTVIEWWIDGET_H

#include "../../guiglobal.h"
#include "../../framework/dialogwidget.h"

#include <qf/core/exception.h>
#include <qf/core/utils/treetable.h>

#include <QScrollArea>
#include <QFrame>
#include <QTransform>

class QLineEdit;
class QSpinBox;
class QPrinter;
class QPrintPreviewWidget;

namespace qf {
namespace gui {

class Action;
class StatusBar;

namespace reports {

class ReportItemMetaPaint;
class ReportItemMetaPaintReport;
class ReportItemMetaPaintFrame;
class ReportPainter;
class ReportProcessor;


class QFGUI_DECL_EXPORT  ReportViewWidget : public qf::gui::framework::DialogWidget
{
	Q_OBJECT
private:
	typedef qf::gui::framework::DialogWidget Super;
public:
	ReportViewWidget(QWidget *parent = nullptr);
	~ReportViewWidget() override;

	/**
	 * @brief showReport
	 * @param parent
	 * @param report_qml_file
	 * @param single_table_data
	 * @param window_title
	 * @param config_persistent_id
	 * @param report_init_properties
	 * @return true if report was printed
	 */
	static bool showReport(QWidget *parent
			, const QString &report_qml_file
			, const QVariant &single_table_data
			, const QString &window_title = tr("Report preview")
			, const QString &config_persistent_id = QString()
			, const QVariantMap &report_init_properties = QVariantMap());
	static bool showReport2(QWidget *parent
			, const QString &report_qml_file
			, const QVariantMap &multiple_table_data
			, const QString &window_title = tr("Report preview")
			, const QString &persistent_settings_id = QString()
			, const QVariantMap &report_init_properties = QVariantMap());

protected:
	QPrintPreviewWidget *m_printPreviewWidget = nullptr;
protected:
	class ScrollArea;
	class PainterWidget;

	PainterWidget *m_painterWidget;
	ScrollArea *m_scrollArea;

	int m_currentPageNo;
	qreal m_scale;

	int m_whenRenderingSetCurrentPageTo;

	QLineEdit *edCurrentPage;

	ReportItemMetaPaint *m_selectedItem;
	QTransform m_painterInverseMatrix;

	ReportProcessor *m_reportProcessor = nullptr;

	static const int PageBorder = 5;

	qf::gui::StatusBar *m_statusBar;
private:
	void onPrintPreviewPaintRequested(QPrinter *printer);

	void selectItem(const QPointF &p);
	ReportItemMetaPaint* selectItem_helper(ReportItemMetaPaint *it, const QPointF &p);
protected:
	qf::gui::StatusBar* statusBar();
	QSpinBox *zoomStatusSpinBox;

	/// nastavi painteru scale a offset
	void setupPainter(ReportPainter *p);
	//! nastavi velikost widgetu podle rozmeru aktualni stranky a aktualniho zvetseni.
	void setupPainterWidgetSize();

	void refreshWidget();
	void refreshActions();

	void print();
	void exportPdf(const QString &file_name);
	QString exportHtml();
signals:
	void sqlValueEdited(const QString &sql_id, const QVariant &val);
protected slots:
	void edCurrentPageEdited();
	void onPageProcessed();
	void scrollToPrevPage() {view_prevPage(ScrollToPageEnd);}
	void scrollToNextPage() {view_nextPage(ScrollToPageTop);}
	void setScaleProc(int proc) {setScale(proc * 0.01);}
public:
	enum PageScrollPosition {ScrollToPageTop, ScrollToPageEnd};
public slots:
	void print(QPrinter *printer); /// slot ktery potrebuje print preview
	/// prerendruje report
	//void render();
	//! Zacne prekladat report a jak pribyvaji stranky, zobrazuji se ve view, nemuzu pro to pouzit specialni thread,
	//! protoze QFont musi byt pouzivan v GUI threadu, tak prekladam stranku po strance pomoci QTimer::singleShot()
	void processReport();

	virtual void file_print();
	virtual void file_printPreview();
	virtual void file_export_pdf(bool open = true);
	virtual void file_export_html();

	void view_nextPage(PageScrollPosition scroll_pos = ScrollToPageTop);
	void view_prevPage(PageScrollPosition scroll_pos = ScrollToPageTop);
	void view_firstPage();
	void view_lastPage();
	void view_zoomIn(const QPoint &center_pos = QPoint());
	void view_zoomOut(const QPoint &center_pos = QPoint());
	void view_zoomToFitWidth();
	void view_zoomToFitHeight();
	void zoomOnWheel(int delta, const QPoint &pos);
public:
	void setVisible(bool visible) override;

	ReportItemMetaPaintReport* document(bool throw_exc = true);
	void setTableData(const QString &key, const qf::core::utils::TreeTable &table_data);
	Q_INVOKABLE void setTableData(const QString &key, const QVariant &table_data);
	Q_INVOKABLE void setTableData(const QVariant &data) {
		setTableData(QString(), data);
	}

	Q_SLOT void setReport(const QString &file_name, const QVariantMap &report_init_properties = QVariantMap());

	/// stranky se pocitaji od 0
	int currentPageNo() const {return m_currentPageNo;}
	/// stranky se pocitaji od 0
	void setCurrentPageNo(int pg_no);
	int pageCount();
	ReportItemMetaPaintFrame *currentPage();
	/// return NULL if such a page does not exist.
	ReportItemMetaPaintFrame *getPage(int page_no);

	qreal scale() const {return m_scale;}
	void setScale(qreal _scale);

	void settleDownInDialog(qf::gui::dialogs::Dialog *dlg) override;
	ActionMap createActions() override;

	ReportItemMetaPaint* selectedItem() const {return m_selectedItem;}
	virtual void prePrint() {}
	void print(QPrinter &printer, const QVariantMap &options = QVariantMap());
	Q_SIGNAL void reportPrinted(int printer_output_format);
private:
	ReportProcessor* reportProcessor();
private:
	QLineEdit *m_edCurrentPage = nullptr;
};

class ReportViewWidget::PainterWidget : public QWidget
{
	Q_OBJECT
protected:
	void mousePressEvent(QMouseEvent *e) override;
	//virtual void wheelEvent(QWheelEvent *event);
	void paintEvent(QPaintEvent *event) override;
	ReportViewWidget* reportViewWidget();
	/// screen dots per mm
signals:
	void sqlValueEdited(const QString &sql_id, const QVariant &val);
public:
	PainterWidget(QWidget *parent);
	virtual ~PainterWidget() {}
};

class ReportViewWidget::ScrollArea : public QScrollArea
{
	Q_OBJECT
protected:
	QPoint f_dragMouseStartPos;
	QPoint f_dragViewportStartPos;
protected:
	void keyPressEvent(QKeyEvent *ev) override;
	void keyReleaseEvent(QKeyEvent *ev) override;
	void wheelEvent(QWheelEvent *e) override;
	void mousePressEvent(QMouseEvent *ev) override;
	void mouseReleaseEvent(QMouseEvent *ev) override;
	void mouseMoveEvent(QMouseEvent *ev) override;
	//virtual void dragMoveEvent(QDragMoveEvent *ev);
signals:
	void showNextPage();
	void showPreviousPage();
protected slots:
	void verticalScrollBarValueChanged(int value);
signals:
	void zoomOnWheel(int delta, const QPoint &pos);
public:
	ScrollArea(QWidget *parent);
};

}}}

#endif // QF_GUI_REPORTS_REPORTVIEWWIDGET_H

