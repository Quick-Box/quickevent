#include "awardreportviewwidget.h"
#include "awarddesign.h"
#include "awardtypstrenderer.h"

#include <qf/gui/action.h>
#include <qf/gui/menubar.h>
#include <qf/gui/statusbar.h>
#include <qf/gui/style.h>
#include <qf/gui/toolbar.h>
#include <qf/gui/dialogs/dialog.h>
#include <qf/gui/dialogs/filedialog.h>
#include <qf/gui/framework/cursoroverrider.h>
#include <qf/core/log.h>

#include <QApplication>
#include <QFile>
#include <QImage>
#include <QLabel>
#include <QLineEdit>
#include <QPageSize>
#include <QPainter>
#include <QPixmap>
#include <QPrintDialog>
#include <QPrintPreviewDialog>
#include <QPrinter>
#include <QScrollArea>
#include <QScrollBar>
#include <QSpinBox>
#include <QSvgRenderer>
#include <QTemporaryDir>
#include <QVBoxLayout>

namespace {
// Extra white margin drawn around the page, in device-independent pixels.
constexpr int PAGE_BORDER = 5;

void configurePrinter(QPrinter &printer, qreal page_w, qreal page_h)
{
	printer.setPageSize(QPageSize(QSizeF(page_w, page_h), QPageSize::Millimeter));
	printer.setFullPage(true);
	printer.setPageMargins(QMarginsF(0, 0, 0, 0));
}
}

AwardReportViewWidget::AwardReportViewWidget(const QString &typ_source, const QStringList &image_files,
	const QList<QVariantMap> &pages, QWidget *parent)
	: Super(parent)
	, m_typSource(typ_source)
	, m_imageFiles(image_files)
	, m_pages(pages)
{
	setPersistentSettingsId(QStringLiteral("awardReportView"));

	const QSizeF page_size = AwardDesigner::Design::pageSizeFromTypst(m_typSource);
	m_pageW = page_size.width();
	m_pageH = page_size.height();

	AwardTypstRenderer renderer(m_typSource, m_imageFiles);
	qf::gui::framework::CursorOverrider cov(Qt::WaitCursor);
	QApplication::processEvents(); // paint the wait cursor before the blocking Typst render
	const QList<QByteArray> svgs = renderer.renderToSvgs(m_pages);
	for (const QByteArray &svg : svgs) {
		auto r = QSharedPointer<QSvgRenderer>::create();
		if (r->load(svg))
			m_renderers.append(r);
		else
			qfWarning() << "Cannot load a rendered award SVG page";
	}

	auto *layout = new QVBoxLayout(this);
	layout->setSpacing(0);
	layout->setContentsMargins(0, 0, 0, 0);

	m_scrollArea = new QScrollArea(this);
	m_scrollArea->setWidgetResizable(false);
	m_scrollArea->setAlignment(Qt::AlignCenter);
	m_scrollArea->setBackgroundRole(QPalette::Mid);
	m_pageLabel = new QLabel(m_scrollArea);
	m_pageLabel->setAttribute(Qt::WA_NoSystemBackground);
	m_scrollArea->setWidget(m_pageLabel);

	layout->addWidget(m_scrollArea, 1);
	layout->addWidget(statusBar());
}

AwardReportViewWidget::~AwardReportViewWidget() = default;

void AwardReportViewWidget::showReport(const QString &typ_source, const QStringList &image_files,
	const QList<QVariantMap> &pages, QWidget *parent)
{
	auto *w = new AwardReportViewWidget(typ_source, image_files, pages);
	w->setWindowTitle(tr("Awards"));

	qf::gui::dialogs::Dialog dlg(parent);
	dlg.setWindowFlag(Qt::WindowMaximizeButtonHint, true);
	dlg.setCentralWidget(w);
	dlg.exec();
}

qf::gui::StatusBar *AwardReportViewWidget::statusBar()
{
	if (!m_statusBar) {
		m_statusBar = new qf::gui::StatusBar(nullptr);
		m_zoomStatusSpinBox = new QSpinBox();
		m_zoomStatusSpinBox->setSingleStep(10);
		m_zoomStatusSpinBox->setMinimum(10);
		m_zoomStatusSpinBox->setMaximum(1000000);
		m_zoomStatusSpinBox->setPrefix(QStringLiteral("zoom: "));
		m_zoomStatusSpinBox->setSuffix(QStringLiteral("%"));
		m_zoomStatusSpinBox->setAlignment(Qt::AlignRight);
		m_statusBar->addWidget(m_zoomStatusSpinBox);
		connect(m_zoomStatusSpinBox, &QSpinBox::valueChanged, this, &AwardReportViewWidget::setScaleProc);
	}
	return m_statusBar;
}

void AwardReportViewWidget::settleDownInDialog(qf::gui::dialogs::Dialog *dlg)
{
	qf::gui::Action *act_file = dlg->menuBar()->actionForPath("file");
	act_file->setText(tr("&File"));
	act_file->addActionInto(action("file.print"));
	act_file->addActionInto(action("file.printPreview"));
	act_file->addSeparatorInto();
	act_file->addActionInto(action("file.export.pdf"));

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

	connect(m_edCurrentPage, &QLineEdit::editingFinished, this, [this]() {
		const int pg = m_edCurrentPage->text().split('/').value(0).toInt() - 1;
		setCurrentPageNo(pg);
	});

	setCurrentPageNo(0);
	setScale(1.0);
}

AwardReportViewWidget::ActionMap AwardReportViewWidget::createActions()
{
	ActionMap ret;
	{
		auto *a = new qf::gui::Action(tr("First page"), this);
		a->setIcon(qf::gui::Style::icon("skip-back"));
		ret[QStringLiteral("view.firstPage")] = a;
		connect(a, &QAction::triggered, this, &AwardReportViewWidget::view_firstPage);
	}
	{
		auto *a = new qf::gui::Action(tr("Prev page"), this);
		a->setIcon(qf::gui::Style::icon("step-back"));
		ret[QStringLiteral("view.prevPage")] = a;
		connect(a, &QAction::triggered, this, &AwardReportViewWidget::view_prevPage);
	}
	{
		auto *a = new qf::gui::Action(tr("Next page"), this);
		a->setIcon(qf::gui::Style::icon("step-forward"));
		ret[QStringLiteral("view.nextPage")] = a;
		connect(a, &QAction::triggered, this, &AwardReportViewWidget::view_nextPage);
	}
	{
		auto *a = new qf::gui::Action(tr("Last page"), this);
		a->setIcon(qf::gui::Style::icon("skip-forward"));
		ret[QStringLiteral("view.lastPage")] = a;
		connect(a, &QAction::triggered, this, &AwardReportViewWidget::view_lastPage);
	}
	{
		auto *a = new qf::gui::Action(tr("Zoom in"), this);
		a->setIcon(qf::gui::Style::icon("zoom-in"));
		ret[QStringLiteral("view.zoomIn")] = a;
		connect(a, &QAction::triggered, this, &AwardReportViewWidget::view_zoomIn);
	}
	{
		auto *a = new qf::gui::Action(tr("Zoom out"), this);
		a->setIcon(qf::gui::Style::icon("zoom-out"));
		ret[QStringLiteral("view.zoomOut")] = a;
		connect(a, &QAction::triggered, this, &AwardReportViewWidget::view_zoomOut);
	}
	{
		auto *a = new qf::gui::Action(tr("Zoom to fit width"), this);
		a->setIcon(qf::gui::Style::icon("zoom-fitwidth"));
		ret[QStringLiteral("view.zoomFitWidth")] = a;
		connect(a, &QAction::triggered, this, &AwardReportViewWidget::view_zoomToFitWidth);
	}
	{
		auto *a = new qf::gui::Action(tr("Zoom to fit height"), this);
		a->setIcon(qf::gui::Style::icon("zoom-fitheight"));
		ret[QStringLiteral("view.zoomFitHeight")] = a;
		connect(a, &QAction::triggered, this, &AwardReportViewWidget::view_zoomToFitHeight);
	}
	{
		auto *a = new qf::gui::Action(tr("&Print"), this);
		a->setIcon(qf::gui::Style::icon("printer"));
		ret[QStringLiteral("file.print")] = a;
		connect(a, &QAction::triggered, this, &AwardReportViewWidget::file_print);
	}
	{
		auto *a = new qf::gui::Action(tr("Print pre&view"), this);
		a->setIcon(qf::gui::Style::icon("print-preview"));
		ret[QStringLiteral("file.printPreview")] = a;
		connect(a, &QAction::triggered, this, &AwardReportViewWidget::file_printPreview);
	}
	{
		auto *a = new qf::gui::Action(tr("Export PD&F"), this);
		a->setIcon(qf::gui::Style::icon("acrobat"));
		a->setToolTip(tr("Export in the Adobe Acrobat PDF format"));
		ret[QStringLiteral("file.export.pdf")] = a;
		connect(a, &QAction::triggered, this, &AwardReportViewWidget::file_exportPdf);
	}
	return ret;
}

void AwardReportViewWidget::setCurrentPageNo(int pg_no)
{
	if (pg_no >= pageCount() || pg_no < 0)
		pg_no = 0;
	m_currentPageNo = pg_no;
	refreshCurrentPage();
	refreshWidget();
}

void AwardReportViewWidget::setScale(qreal scale)
{
	if (scale <= 0)
		return;
	m_scale = scale;
	refreshCurrentPage();
	refreshWidget();
}

void AwardReportViewWidget::refreshCurrentPage()
{
	if (m_currentPageNo < 0 || m_currentPageNo >= m_renderers.size()) {
		m_pageLabel->clear();
		m_pageLabel->resize(0, 0);
		return;
	}
	const qreal px_per_mm = pxPerMm();
	const int w = qRound(m_pageW * px_per_mm * m_scale);
	const int h = qRound(m_pageH * px_per_mm * m_scale);
	if (w <= 0 || h <= 0)
		return;

	QImage img(w + 2 * PAGE_BORDER, h + 2 * PAGE_BORDER, QImage::Format_ARGB32_Premultiplied);
	img.fill(Qt::transparent);
	QPainter p(&img);
	p.fillRect(QRect(PAGE_BORDER, PAGE_BORDER, w, h), Qt::white);
	m_renderers.at(m_currentPageNo)->render(&p, QRectF(PAGE_BORDER, PAGE_BORDER, w, h));
	p.end();

	m_pageLabel->setPixmap(QPixmap::fromImage(img));
	m_pageLabel->resize(img.size());
}

void AwardReportViewWidget::refreshWidget()
{
	if (m_edCurrentPage)
		m_edCurrentPage->setText(QString::number(currentPageNo() + 1) + "/" + QString::number(pageCount()));
	if (m_zoomStatusSpinBox) {
		QSignalBlocker blocker(m_zoomStatusSpinBox);
		m_zoomStatusSpinBox->setValue((int)(scale() * 100));
	}
	refreshActions();
}

void AwardReportViewWidget::refreshActions()
{
	const int pgno = currentPageNo();
	const int pgcnt = pageCount();
	const bool has_pages = pgcnt > 0;
	action("file.print")->setEnabled(has_pages);
	action("file.printPreview")->setEnabled(has_pages);
	action("file.export.pdf")->setEnabled(has_pages);
	action("view.firstPage")->setEnabled(pgno > 0 && has_pages);
	action("view.prevPage")->setEnabled(pgno > 0 && has_pages);
	action("view.nextPage")->setEnabled(pgno < pgcnt - 1);
	action("view.lastPage")->setEnabled(pgno < pgcnt - 1);
	action("view.zoomIn")->setEnabled(has_pages);
	action("view.zoomOut")->setEnabled(has_pages);
	action("view.zoomFitWidth")->setEnabled(has_pages);
	action("view.zoomFitHeight")->setEnabled(has_pages);
}

void AwardReportViewWidget::view_firstPage()
{
	setCurrentPageNo(0);
}

void AwardReportViewWidget::view_prevPage()
{
	if (currentPageNo() > 0)
		setCurrentPageNo(currentPageNo() - 1);
}

void AwardReportViewWidget::view_nextPage()
{
	if (currentPageNo() < pageCount() - 1)
		setCurrentPageNo(currentPageNo() + 1);
}

void AwardReportViewWidget::view_lastPage()
{
	if (pageCount() > 0)
		setCurrentPageNo(pageCount() - 1);
}

void AwardReportViewWidget::view_zoomIn()
{
	setScale(scale() * 1.33);
}

void AwardReportViewWidget::view_zoomOut()
{
	setScale(scale() / 1.33);
}

void AwardReportViewWidget::zoomToFit(qreal page_mm, qreal viewport_px)
{
	if (page_mm <= 0)
		return;
	const qreal page_px = page_mm * pxPerMm() + 2 * PAGE_BORDER;
	setScale(viewport_px / page_px * 0.98);
}

void AwardReportViewWidget::view_zoomToFitWidth()
{
	zoomToFit(m_pageW, m_scrollArea->viewport()->width());
}

void AwardReportViewWidget::view_zoomToFitHeight()
{
	zoomToFit(m_pageH, m_scrollArea->viewport()->height());
}

void AwardReportViewWidget::print(QPrinter &printer)
{
	QPainter painter;
	if (!painter.begin(&printer)) {
		qfWarning() << "Cannot start printing the award document";
		return;
	}
	const QRectF target = printer.pageRect(QPrinter::DevicePixel);
	for (int i = 0; i < m_renderers.size(); ++i) {
		if (i > 0)
			printer.newPage();
		m_renderers.at(i)->render(&painter, target);
	}
	painter.end();
}

void AwardReportViewWidget::file_print()
{
	QPrinter printer(QPrinter::HighResolution);
	printer.setOutputFormat(QPrinter::NativeFormat);
	configurePrinter(printer, m_pageW, m_pageH);

	QPrintDialog dlg(&printer, this);
	if (dlg.exec() != QDialog::Accepted)
		return;
	print(printer);
}

void AwardReportViewWidget::file_printPreview()
{
	QPrinter printer(QPrinter::HighResolution);
	printer.setOutputFormat(QPrinter::NativeFormat);
	configurePrinter(printer, m_pageW, m_pageH);

	QPrintPreviewDialog preview(&printer, this);
	connect(&preview, &QPrintPreviewDialog::paintRequested, this, [this](QPrinter *p) { print(*p); });
	preview.exec();
}

void AwardReportViewWidget::file_exportPdf()
{
	QString fn = qf::gui::dialogs::FileDialog::getSaveFileName(this,
		tr("Export PDF"), QString(), QStringLiteral("*.pdf"));
	if (fn.isEmpty())
		return;
	if (!fn.endsWith(QStringLiteral(".pdf"), Qt::CaseInsensitive))
		fn += QStringLiteral(".pdf");

	// Use Typst's native PDF output for best (vector) quality.
	AwardTypstRenderer renderer(m_typSource, m_imageFiles);
	QTemporaryDir temp_dir;
	qf::gui::framework::CursorOverrider cov(Qt::WaitCursor);
	QApplication::processEvents(); // paint the wait cursor before the blocking Typst render
	const QString pdf_path = renderer.renderToPdf(m_pages, temp_dir);
	if (pdf_path.isEmpty()) {
		qfWarning() << "Cannot render the award PDF document";
		return;
	}
	QFile::remove(fn);
	if (!QFile::copy(pdf_path, fn))
		qfWarning() << "Cannot write the award PDF to" << fn;
}
