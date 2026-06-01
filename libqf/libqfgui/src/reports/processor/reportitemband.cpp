#include "reportitemband.h"

#include "reportprocessor.h"
#include "reportitemdetail.h"
#include "reportitempara.h"
#include "banddatamodel.h"

#include <qf/core/log.h>
#include <qf/core/assert.h>
#include <qf/core/string.h>


using namespace qf::gui::reports;

ReportItemBand::ReportItemBand(ReportItem *parent)
	: Super(parent)
{
	qfLogFuncFrame() << this;
}

ReportItemBand::~ReportItemBand()
{
	qfLogFuncFrame() << this;
	QF_SAFE_DELETE(m_model);
}

void ReportItemDetail::resetCurrentIndex()
{
	m_currentIndex = -1;
}

void ReportItemBand::setModelData(QVariant d)
{
	if (m_data == d)
		return;
	m_data = d;
	QF_SAFE_DELETE(m_model)
	emit modelDataChanged(d);

}

BandDataModel *ReportItemBand::model()
{
	//qfLogFuncFrame() << this << "m_model:" << m_model << "isDataValid:" << (m_model? m_model->isDataValid(): false);
	if(!m_model || !m_model->isDataValid()) {
		ReportItemDetail *parent_detail = nullptr;
		QVariant dta = modelData();
		//qfInfo() << this << "creating data model";
		//qfInfo() << "1:" << dta;
		if(!dta.isValid() || dta.userType() == QMetaType::QString) {
			QString data_key = dta.toString();
			ReportItemBand *pr = parentBand();
			//qfInfo() << data_key << pr;
			if(pr) {
				BandDataModel *dm = pr->model();
				parent_detail = pr->detail();
				if(dm && parent_detail) {
					dta = dm->table(parent_detail->currentIndex(), data_key);
				}
			}
			else {
				/// take data from report processor
				ReportProcessor *proc = processor(!qf::core::Exception::Throw);
				if(proc) {
					dta = proc->data(data_key);
				}
			}
		}
		//qfInfo() << "2:" << dta;
		QF_SAFE_DELETE(m_model);
		m_model = BandDataModel::createFromData(dta, this);
		//qfError() << m_model;
		QF_ASSERT(m_model != nullptr, "Bad data!", return m_model);
		if(parent_detail)
			connect(parent_detail, &ReportItemDetail::currentIndexChanged, m_model, &BandDataModel::invalidateData);
		//emit modelLoadedChanged(true);
	}
	return m_model;
}

bool ReportItemBand::isModelLoaded()
{
	return (m_model != nullptr && m_model->isDataValid());
}

QVariant ReportItemBand::data(const QString &field_name, int role)
{
	qfLogFuncFrame() << "field_name:" << field_name;
	QVariant ret;
	BandDataModel *m = model();
	if(m) {
		ret = m->tableData(field_name, (BandDataModel::DataRole)role);
	}
	else {
		qfWarning() << "Band has not valid data model.";
	}
	return ret;
}

ReportItem::PrintResult ReportItemBand::printMetaPaint(QPaintDevice *paint_device, ReportItemMetaPaint *out, const ReportItem::Rect &bounding_rect)
{
	qfLogFuncFrame() << this;
	if(isCreateFromData() && !detail()) {
		createChildItemsFromData();
	}
	if(isHeaderOnBreak()) {
		/// print everything except of detail again
		ReportItemDetail *it_det = detail();
		if(it_det)
			it_det->resetIndexToPrintRecursively(ReportItem::IncludingParaTexts);
		m_indexToPrint = 0;
	}
	auto *m = model(); // load model before rendering
	PrintResult res = PrintResult::createPrintFinished();
	qfDebug() << "model:" << m << "row count:" << m->rowCount();
	if(m->rowCount() > 0) {
		res = Super::printMetaPaint(paint_device, out, bounding_rect);
	}
	qfDebug() << "\tRETURN:" << res.toString();
	return res;
}

void ReportItemBand::resetIndexToPrintRecursively(bool including_para_texts)
{
	qfLogFuncFrame() << objectName();// << currentIndex();
	Super::resetIndexToPrintRecursively(including_para_texts);
	//dataTableLoaded = false;
	ReportItemDetail *det = detail();
	if(det) {
		det->resetCurrentIndex();
	}
}

bool ReportItemBand::canBreak()
{
	bool ret = Super::canBreak();
	if(ret) {
		if(keepFirst() > 0) {
			ReportItemDetail *det = detail();
			if(det) {
				ret = (det->currentIndex() > keepFirst());
			}
		}
	}
	return ret;
}

ReportItemDetail *ReportItemBand::detail()
{
	auto *ret = findChild<ReportItemDetail*>(QString(), Qt::FindDirectChildrenOnly);
	return ret;
}

void ReportItemBand::createChildItemsFromData()
{
	BandDataModel *mod = model();
	qfLogFuncFrame() << "model:" << mod;
	if(mod) {
		auto *brush_lightgray = new style::Brush(this);
		{
			auto *c = new style::Color(brush_lightgray);
			c->setDefinition(QColor(Qt::lightGray));
			brush_lightgray->setColor(c);
		}
		auto *it_header_frm = new ReportItemFrame(this);
		{
			it_header_frm->setWidth("%");
			it_header_frm->setLayout(ReportItemFrame::LayoutHorizontal);
			it_header_frm->setFill(brush_lightgray);
			//it_header_frm->setHinset(3);
			//it_header_frm->setVinset(3);
		}
		addItem(it_header_frm);
		auto *pen_black1 = new style::Pen(this);
		{
			pen_black1->setWidth(1);
			auto *c = new style::Color(pen_black1);
			c->setDefinition(QColor(Qt::black));
			pen_black1->setColor(c);
		}
		int col_cnt = mod->columnCount();
		for(int i=0; i<col_cnt; i++) {
			auto *it_para = new ReportItemPara(it_header_frm);
			it_para->setBorder(pen_black1);
			it_para->setWidth(mod->headerData(i, Qt::SizeHintRole));
			it_para->setText(mod->headerData(i).toString());
			it_para->setHinset(1);
			it_header_frm->addItem(it_para);
		}
		auto *it_det = new ReportItemDetail(this);
		{
			it_det->setWidth("%");
			it_det->setLayout(ReportItemFrame::LayoutHorizontal);
			it_det->setExpandChildFrames(true);
		}
		addItem(it_det);
		for(int i=0; i<col_cnt; i++) {
			auto *it_para = new ReportItemPara(it_det);
			it_para->setBorder(pen_black1);
			it_para->setWidth(mod->headerData(i, Qt::SizeHintRole));
			it_para->setHinset(1);
			auto fn = [mod, it_det, i]()->QString {
				QString ret = mod->dataByIndex(it_det->currentIndex(), i).toString();
				return ret;
			};
			it_para->setGetTextCppFunction(fn);
			it_det->addItem(it_para);
		}
	}
	//dumpObjectTree();
}


