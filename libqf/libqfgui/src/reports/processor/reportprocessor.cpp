#include "reportprocessor.h"
#include "reportitemreport.h"
#include "reportpainter.h"
//#include "../../framework/application.h"
#include "../../framework/plugin.h"

#include <qf/core/utils/fileutils.h>
#include <qf/core/log.h>
#include <qf/core/utils.h>

#include <QSet>
#include <QJsonDocument>
#include <QQmlEngine>
#include <QDomElement>
#include <QQmlContext>

//#define QF_TIMESCOPE_ENABLED
#include <qf/core/utils/timescope.h>

namespace qfu = qf::core::utils;

using namespace qf::gui::reports;

//===================================================
// ReportProcessor
//===================================================
QString ReportProcessor::HTML_ATTRIBUTE_ITEM = QStringLiteral("__qf_qml_report_item");
QString ReportProcessor::HTML_ATTRIBUTE_LAYOUT = QStringLiteral("__qf_qml_report_layout");

ReportProcessor::ReportProcessor(QObject *parent)
	: QObject(parent)
{
	qfLogFuncFrame();

	m_qmlEngine = nullptr;
	//--f_searchDirs = search_dirs;
	m_designMode = false;
	setProcessedPageNo(0);
}

ReportProcessor::~ReportProcessor()
{
	qfLogFuncFrame() << this << "m_documentInstanceRoot:" << m_documentInstanceRoot;
	QF_SAFE_DELETE(m_documentInstanceRoot);
	//qfInfo() << "delete ReportProcessor" << this;
}

void ReportProcessor::reset()
{
	qfLogFuncFrame();
	QF_SAFE_DELETE(m_documentInstanceRoot);
	QF_SAFE_DELETE(m_processorOutput);
}

bool ReportProcessor::setReport(const QString &rep_file_name, const QVariantMap &report_init_properties)
{
	QF_TIME_SCOPE("ReportProcessor::setReport()");
	m_reportInitProperties = report_init_properties;
	QF_SAFE_DELETE(m_reportDocumentComponent)
	m_reportDocumentComponent = new ReportDocument(qmlEngine(true), this);
	//QString fn = rep_file_name;
	//if(fn.startsWith(QLatin1String("qrc://")))
	//	fn = fn.mid(3);
	m_reportDocumentComponent->setFileName(rep_file_name);
	if(m_reportDocumentComponent->isError()) {
		qfError() << "Erorr loading report component:" << rep_file_name << m_reportDocumentComponent->errorString();
		return false;
	}
	if(!m_reportDocumentComponent->isReady()) {
		qfError() << tr("QML component") << m_reportDocumentComponent->url().toString() << "cannot be loaded asynchronously";
		return false;
	}
	return true;
}

QUrl ReportProcessor::reportUrl() const
{
	QUrl ret;
	if(m_reportDocumentComponent) {
		ret = m_reportDocumentComponent->url();
	}
	return ret;
}

void ReportProcessor::setTableData(const QString &key, const QVariant &table_data)
{
	qfu::TreeTable tt(table_data);
	setTableData(key, tt);
}

void ReportProcessor::setTableData(const QString &key, const qf::core::utils::TreeTable &table_data)
{
	QVariant v = QVariant::fromValue(table_data);
	setData(key, v);
}

void ReportProcessor::setData(const QString &key, const QVariant &data)
{
	//qfInfo() << "ReportProcessor _data:" << _data.toString().mid(0, 100);
	m_data[key] = data;
	//qfInfo() << "SET reportProcessor data:" << m_data.toString();
}

ReportItemReport* ReportProcessor::documentInstanceRoot()
{
	QF_TIME_SCOPE("ReportProcessor::documentInstanceRoot()");
	if(!m_documentInstanceRoot) {
		QObject *o;
		{
			QF_TIME_SCOPE("ReportProcessor::documentInstanceRoot() - creating report object begin");
			o = m_reportDocumentComponent->beginCreate(qmlEngine()->rootContext());
			m_documentInstanceRoot = qobject_cast<ReportItemReport*>(o);
		}
		if(!m_documentInstanceRoot) {
			qfError() << "Error creating root object from component:" << m_reportDocumentComponent << m_reportDocumentComponent->url().toString();
			qfError() << "Created object:" << o;
			Q_FOREACH(auto err, m_reportDocumentComponent->errors())
				qfError() << err.toString();
			QF_SAFE_DELETE(o)
		}
		else {
			QF_TIME_SCOPE("ReportProcessor::documentInstanceRoot() - creating report object finish");
			QMapIterator<QString, QVariant> it(m_reportInitProperties);
			while(it.hasNext()) {
				it.next();
				QByteArray ba = it.key().toLatin1();
				if(!m_documentInstanceRoot->setProperty(ba.constData(), it.value()))
					qfWarning() << "Cannot set report root property" << ba << ", root element should have this property defined explicitly.";
			}
			m_reportDocumentComponent->completeCreate();
			m_documentInstanceRoot->setReportProcessor(this);
			style::Text *st = m_documentInstanceRoot->textStyle();
			if(!st) {
				qfWarning() << "Report document has not the textStyle property set to valid TextStyle object, "
							   "Para.text will not have implicit TextStyle definition.";
			}
			style::Sheet *ss = m_documentInstanceRoot->styleSheet();
			if(ss)
				ss->createStyleCache();
			setDesignMode(m_documentInstanceRoot->debugLevel() > 0);
			m_documentInstanceRoot->setCreated(true);
		}
	}
	return m_documentInstanceRoot;
}

void ReportProcessor::process(QPaintDevice *paint_device, ReportProcessor::ProcessorMode mode)
{
	qfLogFuncFrame() << "mode:" << mode;
	QF_TIME_SCOPE("ReportProcessor::process");
	if(mode == FirstPage || mode == AllPages) {
		setProcessedPageNo(0);
		QF_SAFE_DELETE(m_processorOutput);
		if(documentInstanceRoot()) {
			m_processorOutput = new ReportItemMetaPaintReport(documentInstanceRoot());
			m_singlePageProcessResult = ReportItem::PrintResult::createPrintAgain();
		}
	}
	ReportItemMetaPaint mpit;
	//context().dump();
	while(m_singlePageProcessResult.isPrintAgain()) {
		{
			QF_TIME_SCOPE("processing page");
			m_singlePageProcessResult = processPage(paint_device, &mpit);
			qfDebug() << "singlePageProcessResult:" << m_singlePageProcessResult.toString();
		}
		//qfDebug().color(QFLog::Yellow) << context().styleCache().toString();
		//mpit.dump();
		ReportItemMetaPaint *it = mpit.firstChild();
		if(it) {
			it->setParent(m_processorOutput);
			if(mode == FirstPage || mode == SinglePage) {
				emit pageProcessed();
				if(m_singlePageProcessResult.isPrintAgain()) {
					setProcessedPageNo(processedPageNo() + 1);
				}
				//qfInfo() << "pageProcessed:" << fProcessedPageNo;
				break;
			}
			if(m_singlePageProcessResult.isPrintAgain()) {
				setProcessedPageNo(processedPageNo() + 1);
			}
			else {
				break;
			}
		}
		else {
			break;
		}
	}
}

ReportItem::PrintResult ReportProcessor::processPage(QPaintDevice *paint_device, ReportItemMetaPaint *out)
{
	qfLogFuncFrame();
	QF_TIME_SCOPE("ReportProcessor::processPage()");
	ReportItem::PrintResult res;
	if(documentInstanceRoot()) {
		QF_TIME_SCOPE("ReportProcessor::processPage - documentInstanceRoot()->printMetaPaint()");
		res = documentInstanceRoot()->printMetaPaint(paint_device, out, ReportItem::Rect());
		qfDebug() << "\tres:" << res.toString();
	}
	return res;
}

void ReportProcessor::processHtml(QDomElement & el_body, const HtmlExportOptions &opts)
{
	ReportItemReport *root_item = documentInstanceRoot();
	if(root_item == nullptr)
		return;
	root_item->resetIndexToPrintRecursively(ReportItem::IncludingParaTexts);
	root_item->printHtml(el_body);
	removeRedundantDivs(el_body);
	if(opts.isConvertBandsToTables())
		convertBandsToTables(el_body);
}
/*
void ReportProcessor::fixTableTags(QDomElement & _el)
{
	QDomElement el(_el);
	bool is_table_row = false;
	bool is_table_header_row = false;
	QString attr = el.attribute("__table");
	if(!attr.isEmpty()) {
		attr = attr.mid(QString("__fakeBand").length());
		if(attr == "Table") {
			el.setTagName("table");
			el.setAttribute("border", 1);
		}
		else if(attr == "Detail") {el.setTagName("tr"); is_table_row = true;}
		else if(attr == "HeaderRow") {el.setTagName("tr"); is_table_header_row = true;}
		else if(attr == "FooterRow") {el.setTagName("tr"); is_table_header_row = true;}
	}
	for(QDomElement el1 = el.firstChildElement(); !el1.isNull(); el1 = el1.nextSiblingElement()) {
		fixTableTags(el1);
		if(is_table_row) el1.setTagName("td");
		if(is_table_header_row) el1.setTagName("th");
	}
}
*/
QDomElement ReportProcessor::removeRedundantDivs(QDomElement & _el)
{
	qfLogFuncFrame() << _el.tagName() << "children cnt:" << _el.childNodes().count();
	QDomElement el(_el);
	//qfDebug() << "\t path:" << el.path();
	/// div is superfluous if it has zero or one child and no attributes
	while(el.tagName() == "div" && el.childNodes().count() <= 1 && el.attributes().isEmpty()) {
		QDomNode parent_nd = el.parentNode();
		QDomElement el_child = el.childNodes().at(0).toElement();
		if(el_child.isNull())
			break;
		if(!el_child.isNull() && !parent_nd.isNull()) {
			qfDebug() << "\t child el:" << el_child.tagName() << "children cnt:" << el_child.childNodes().count();
			parent_nd.replaceChild(el_child, el).toElement();
			el = el_child;
			qfDebug() << "\t new el:" << el.tagName() << "children cnt:" << el.childNodes().count();
		}
	}
	qfDebug() << "\t checking children of el:" << el.tagName() << "children cnt:" << el.childNodes().count();
	for(QDomElement el1 = el.firstChildElement(); !el1.isNull(); el1 = el1.nextSiblingElement()) {
		el1 = removeRedundantDivs(el1);
	}
	return el;
}

QDomElement ReportProcessor::convertBandsToTables(QDomElement & _el)
{
	qfLogFuncFrame() << _el.tagName() << "children cnt:" << _el.childNodes().count();
	QDomElement el(_el);
	QDomNode parent_nd = el.parentNode();
	if(!parent_nd.isNull()) {
		if(el.tagName() == "div") {
			if(el.attribute(HTML_ATTRIBUTE_ITEM) == QStringLiteral("band")) {
				// change divs to table for Band
				//qfWarning() << "BAND";
				QDomElement el_table = el.ownerDocument().createElement("table");
				QDomNode old_el = parent_nd.replaceChild(el_table, el);
				el_table.setAttribute("border", 1);
				QDomElement el_caption = el.ownerDocument().createElement("caption");
				el_table.appendChild(el_caption);

				while(true) {
					QDomElement el1 = old_el.firstChildElement();
					if(el1.isNull())
						break;
					QDomElement el_tr = convertHorizontalDivToTableRow(el1);
					if(el_tr.isNull()) {
						el_caption.appendChild(el1);
					}
					else {
						el_table.appendChild(el_tr);
						old_el.removeChild(el1);
					}
				}
				el = el_table;
			}
			else {
				QDomElement el_tr = convertHorizontalDivToTableRow(el);
				if(!el_tr.isNull()) {
					QDomElement el_table = el.ownerDocument().createElement("table");
					parent_nd.replaceChild(el_table, el);
					el_table.appendChild(el_tr);
					el = el_table;
				}
			}
		}
	}
	for(QDomElement el1 = el.firstChildElement(); !el1.isNull(); el1 = el1.nextSiblingElement()) {
		el1 = convertBandsToTables(el1);
	}
	return el;
}

QDomElement ReportProcessor::convertHorizontalDivToTableRow(QDomElement &el_div)
{
	QDomElement el_table_row;
	if(el_div.tagName() == QLatin1String("div") && el_div.attribute(HTML_ATTRIBUTE_LAYOUT) == QLatin1String("horizontal")) {
		bool is_detail = el_div.attribute(HTML_ATTRIBUTE_ITEM) == QLatin1String("detail");
		el_table_row = el_div.ownerDocument().createElement("tr");
		while(true) {
			QDomElement el2 = el_div.firstChildElement();
			if(el2.isNull())
				break;
			QString td_tag_name = (is_detail)? QStringLiteral("td"): QStringLiteral("th");
			QDomElement el_td = el_div.ownerDocument().createElement(td_tag_name);
			el_table_row.appendChild(el_td);
			el_td.appendChild(el2);
		}
	}
	return el_table_row;
}

QStringList ReportProcessor::qmlEngineImportPaths()
{
	QStringList lst;
#ifdef Q_OS_UNIX
	lst << QCoreApplication::applicationDirPath() + "/../lib/qml";
#endif
	lst << QCoreApplication::applicationDirPath() + "/qml";
	lst << framework::Plugin::effectiveReportsDir();
	lst << ":/quickevent";
	return lst;
}

QQmlEngine *ReportProcessor::qmlEngine(bool throw_exc)
{
#ifdef USE_APP_ENGINE
	QQmlEngine *ret = nullptr;
	qf::gui::framework::Application *app = qobject_cast<qf::gui::framework::Application*>(QCoreApplication::instance());
	if(throw_exc)
		QF_ASSERT_EX(app != nullptr, "Application is not a type of qf::gui::framework::Application");
	if(app) {
		ret = app->qmlEngine();
		if(throw_exc)
			QF_ASSERT_EX(ret != nullptr, "Application has not QML engine created.");
	}
	return ret;
#else
	Q_UNUSED(throw_exc)
	if(!m_qmlEngine) {
		qfMessage() << "Creating report processor QML engine";
		m_qmlEngine = new QQmlEngine(this);
		m_qmlEngine->rootContext()->setContextProperty("reportProcessor", this);
		m_qmlEngine->rootContext()->setContextProperty("application", QCoreApplication::instance());
		Q_FOREACH(auto path, qmlEngineImportPaths()) {
			qfMessage() << "Adding ReportProcessor QML engine import path:" << path;
			m_qmlEngine->addImportPath(path);
		}
	}
	return m_qmlEngine;
#endif
}

void ReportProcessor::dump()
{
	if(m_processorOutput) m_processorOutput->dump();
}

void ReportProcessor::print(QPrinter &printer, const QVariantMap &options)
{
	qfLogFuncFrame();

	ReportPainter painter(&printer);

	typedef ReportItem::Rect Rect;
	//typedef ReportItem::Size Size;

	int pg_no = options.value("fromPage", 1).toInt() - 1;
	int to_page = options.value("toPage", pageCount()).toInt();
	qfDebug() << "pg_no:" << pg_no << "to_page:" << to_page;
	ReportItemMetaPaintFrame *frm = getPage(pg_no);
	if(frm) {
		Rect r = frm->renderedRect;
		bool landscape = r.width() > r.height();
		if(landscape) printer.setPageOrientation(QPageLayout::Landscape);
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
			if(!frm) break;
			if(pg_no >= to_page) break;
			printer.newPage();
		}
	}
}

ReportItemMetaPaintFrame* ReportProcessor::getPage(int n)
{
	if(!processorOutput()) return nullptr;
	if(n < 0 || n >= processorOutput()->childrenCount()) return nullptr;
	ReportItemMetaPaint *it = processorOutput()->child(n);
	auto *frm	= dynamic_cast<ReportItemMetaPaintFrame*>(it);
	qfDebug() << "\treturn:" << frm;
	return frm;
}

int ReportProcessor::pageCount()
{
	qfLogFuncFrame();
	int ret = 0;
	if(processorOutput()) {
		ret = processorOutput()->childrenCount();
	}
	return ret;
}
