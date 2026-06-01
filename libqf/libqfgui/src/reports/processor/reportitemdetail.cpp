#include "reportitemdetail.h"

#include "reportitemband.h"
#include "banddatamodel.h"

#include <qf/core/log.h>
#include <qf/core/assert.h>
#include <qf/core/string.h>

#include <qf/core/utils/timescope.h>


using namespace qf::gui::reports;

//==========================================================
//                  ReportItemDetail
//==========================================================
ReportItemDetail::ReportItemDetail(ReportItem *parent)
	: Super(parent)
{
	qfLogFuncFrame();
}

ReportItemDetail::~ReportItemDetail()
{
	qfLogFuncFrame();
}

int ReportItemDetail::rowCount()
{
	int ret = 0;
	ReportItemBand *band = parentBand();
	if(band && band->isModelLoaded()) {
		BandDataModel *m = band->model();
		if(m) {
			ret = m->rowCount();
		}
	}
	return ret;
}

QVariant ReportItemDetail::data(int row_no, const QString &field_name, int role)
{
	qfLogFuncFrame() << "row_no:" << row_no << "field_name:" << field_name;
	QVariant ret;
	if(row_no >= 0) {
		ReportItemBand *band = parentBand();
		if(band) {
			BandDataModel *m = band->model();
			if(m) {
				ret = m->dataByName(row_no, field_name, (BandDataModel::DataRole)role);
			}
			else {
				qfWarning() << "Parent Band has not valid data model.";
			}
		}
		else {
			qfWarning() << "Detail without parent Band.";
		}
	}
	else {
		// this is not necessary error
		// when data is not loaded and QML is instantiated then currentIndex == -1 and code goes this way
		// return QString to avoid warning: Unable to assign [undefined] to QString
		// when data is assigned to QML property of type string
		ret = QString();
		//qfWarning() << "row_no:" << row_no;
		//ret = QStringLiteral("BAD_ROW");
	}
	qfDebug() << "\t RETURN:" << ret;
	return ret;
}

QVariant ReportItemDetail::rowData(const QString &field_name, int role)
{
	qfLogFuncFrame() << currentIndex() << field_name;
	return data(currentIndex(), field_name, role);
}

ReportItem::PrintResult ReportItemDetail::printMetaPaint(QPaintDevice *paint_device, ReportItemMetaPaint *out, const ReportItem::Rect &bounding_rect)
{
	qfLogFuncFrame();
	QF_TIME_SCOPE("ReportItemDetail::printMetaPaint");
	auto *band = qobject_cast<ReportItemBand*>(parent());
	BandDataModel *model = nullptr;
	if(band) {
		model = band->model();
		if(model) {
			if(currentIndex() < 0) {
				//qfWarning() << "emit rowCountChanged()";
				//emit rowCountChanged();
				setCurrentIndex(0);
			}
		}
	}
	/*--
	PrintResult res;
	bool design_mode = processor()->isDesignMode(); /// true znamena, zobraz prvni radek, i kdyz tam nejsou data.
	//qfInfo() << "design mode:" << design_mode;
	if(!design_mode && (data_table.isNull() || dataRow().isNull())) {
		/// prazdnej detail vubec netiskni
		res.value = PrintOk;
		return res;
	}
	--*/
	PrintResult res = Super::printMetaPaint(paint_device, out, bounding_rect);
	if(res.isPrintFinished()) {
		if(model) {
			/// take next data row
			int ix = currentIndex() + 1;
			qfDebug() << currentIndex() << "/" << model->rowCount();
			//qfDebug() << model->dump();
			if(ix < model->rowCount()) {
				QF_TIME_SCOPE("ReportItemDetail::printMetaPaint 3");
				setCurrentIndex(ix);
				resetIndexToPrintRecursively(ReportItem::IncludingParaTexts);
				res.setNextDetailRowExists(true);
			}
			else {
				resetCurrentIndex();
			}
		}
	}
	return res;
}
