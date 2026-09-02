#pragma once
#include <qf/gui/framework/dialogwidget.h>

#include <QList>
#include <QSharedPointer>
#include <QString>
#include <QStringList>
#include <QVariantMap>

class QLabel;
class QLineEdit;
class QScrollArea;
class QSpinBox;
class QPrinter;
class QSvgRenderer;

namespace qf::gui { class StatusBar; }

// Preview/print pane for award documents rendered natively by Typst.
// Pages are shown and printed as vector SVG; PDF export uses Typst's own PDF output.
// Mirrors qf::gui::reports::ReportViewWidget's look: File/View menus, page navigation and zoom.
class AwardReportViewWidget : public qf::gui::framework::DialogWidget
{
	Q_OBJECT
	using Super = qf::gui::framework::DialogWidget;
public:
	AwardReportViewWidget(const QString &typ_source, const QStringList &image_files,
		const QList<QVariantMap> &pages, QWidget *parent = nullptr);
	~AwardReportViewWidget() override;

	// Build the pane, wrap it in the standard report dialog and show it modally.
	static void showReport(const QString &typ_source, const QStringList &image_files,
		const QList<QVariantMap> &pages, QWidget *parent = nullptr);

	void settleDownInDialog(qf::gui::dialogs::Dialog *dlg) override;
	ActionMap createActions() override;

public slots:
	void file_print();
	void file_printPreview();
	void file_exportPdf();

	void view_firstPage();
	void view_prevPage();
	void view_nextPage();
	void view_lastPage();
	void view_zoomIn();
	void view_zoomOut();
	void view_zoomToFitWidth();
	void view_zoomToFitHeight();

private:
	void print(QPrinter &printer);

	qf::gui::StatusBar *statusBar();

	int pageCount() const { return m_renderers.size(); }
	int currentPageNo() const { return m_currentPageNo; }
	void setCurrentPageNo(int pg_no);

	qreal scale() const { return m_scale; }
	void setScale(qreal scale);
	void setScaleProc(int proc) { setScale(proc * 0.01); }

	// Screen dots per mm at the current logical resolution, so scale == 1.0 renders the page at physical size.
	qreal pxPerMm() const { return logicalDpiX() / 25.4; }
	void zoomToFit(qreal page_mm, qreal viewport_px);

	void refreshCurrentPage();
	void refreshWidget();
	void refreshActions();

	QString m_typSource;
	QStringList m_imageFiles;
	qreal m_pageW = 210;
	qreal m_pageH = 297;
	QList<QVariantMap> m_pages;
	QList<QSharedPointer<QSvgRenderer>> m_renderers;

	QScrollArea *m_scrollArea = nullptr;
	QLabel *m_pageLabel = nullptr;
	QLineEdit *m_edCurrentPage = nullptr;
	qf::gui::StatusBar *m_statusBar = nullptr;
	QSpinBox *m_zoomStatusSpinBox = nullptr;

	int m_currentPageNo = -1;
	qreal m_scale = 1.0;
};
