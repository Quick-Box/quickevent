//
// Author: Frantisek Vacek <fanda.vacek@volny.cz>, (C) 2006, 2014
//
// Copyright: See COPYING file that comes with this distribution
//

#ifndef QF_GUI_REPORTS_REPORTITEMBAND_H
#define QF_GUI_REPORTS_REPORTITEMBAND_H

#include "reportitemframe.h"
#include "../../guiglobal.h"

namespace qf {
namespace gui {
namespace reports {

class BandDataModel;
class ReportItemDetail;

class QFGUI_DECL_EXPORT ReportItemBand : public ReportItemFrame
{
	Q_OBJECT
	//Q_PROPERTY(qf::gui::reports::ReportItemFrame* header READ header WRITE setHeader NOTIFY headerChanged)
	Q_PROPERTY(bool headerOnBreak READ isHeaderOnBreak WRITE setHeaderOnBreak NOTIFY headerOnBreakChanged)
	Q_PROPERTY(QVariant modelData READ modelData WRITE setModelData NOTIFY modelDataChanged)
	Q_PROPERTY(qf::gui::reports::BandDataModel* dataModel READ model)
	Q_PROPERTY(bool createFromData READ isCreateFromData WRITE setCreateFromData NOTIFY createFromDataChanged)
	//Q_PROPERTY(bool modelLoaded READ modelLoaded NOTIFY modelLoadedChanged)
	Q_CLASSINFO("property.keepFirst.doc", "Number of band details printed, which cannot be splitted by page/column break.")
	Q_PROPERTY(int keepFirst READ keepFirst WRITE setKeepFirst NOTIFY keepFirstChanged)
	Q_PROPERTY(bool htmlExportAsTable READ isHtmlExportAsTable WRITE setHtmlExportAsTable NOTIFY htmlExportAsTableChanged)
private:
	typedef ReportItemFrame Super;
public:
	ReportItemBand(ReportItem *parent = nullptr);
	~ReportItemBand() Q_DECL_OVERRIDE;
public:
	QF_PROPERTY_BOOL_IMPL(h, H, eaderOnBreak)
	QF_PROPERTY_BOOL_IMPL(c, C, reateFromData)
	QF_PROPERTY_IMPL2(int, k, K, eepFirst, 0)
	QF_PROPERTY_BOOL_IMPL2(h, H, tmlExportAsTable, true)
public:
	QVariant modelData() const { return m_data; }
	void setModelData(QVariant d);
	Q_SIGNAL void modelDataChanged(QVariant new_data);
public:
	BandDataModel* model();
	bool isModelLoaded();
	// modelLoaded cannot be used in QML, it was bad pattern, ReportItem has NULL parentBand when Component.onCompleted is called
	//Q_SIGNAL void modelLoadedChanged(bool is_loaded);

	Q_INVOKABLE QVariant data(const QString &field_name, int role = Qt::DisplayRole);
public:
	PrintResult printMetaPaint(QPaintDevice *paint_device, ReportItemMetaPaint *out, const Rect &bounding_rect) Q_DECL_OVERRIDE;
	PrintResult printHtml(HTMLElement &out) Q_DECL_OVERRIDE;

	void resetIndexToPrintRecursively(bool including_para_texts) Q_DECL_OVERRIDE;
	bool canBreak() Q_DECL_OVERRIDE;

protected:
	ReportItemDetail* detail();
	void createChildItemsFromData();
protected:
	BandDataModel *m_model = nullptr;
private:
	QVariant m_data;
};

#if 0

class QFGUI_DECL_EXPORT ReportItemDetail : public ReportItemFrame
{
	Q_OBJECT
	friend class ReportItemBand;
private:
	typedef ReportItemFrame Super;
protected:
	//qf::core::utils::TreeTableRow f_dataRow;
	int f_currentRowNo;

	virtual ReportItemDetail* toDetail() {return this;}
public:
	virtual void resetIndexToPrintRecursively(bool including_para_texts);
	virtual PrintResult printMetaPaint(QPaintDevice *paint_device, ReportItemMetaPaint *out, const Rect &bounding_rect);
	virtual PrintResult printHtml(HTMLElement &out);

	qf::core::utils::TreeTable dataTable();
	qf::core::utils::TreeTableRow dataRow();
	/// cislo prave tisteneho radku, pocitano od nuly.
	int currentRowNo() const {return f_currentRowNo;}
	void resetCurrentRowNo() {f_currentRowNo = 0;}
public:
	ReportItemDetail(ReportItem *parent = nullptr);
	virtual ~ReportItemDetail() {}
};


class QFGUI_DECL_EXPORT ReportItemBand : public ReportItemFrame
{
	Q_OBJECT
	Q_PROPERTY(QString dataSource READ dataSource WRITE setDataSource NOTIFY dataSourceChanged)
	//TODO: introduce dataModel property as ReportBandModel : public QObject ancestor implementing TreeTable interface
	//      Then data property can be removed
	Q_PROPERTY(qf::core::utils::TreeTable data READ data WRITE setData NOTIFY dataChanged)
	Q_PROPERTY(bool headerOnBreak READ isHeaderOnBreak WRITE setHeaderOnBreak NOTIFY headerOnBreakChanged)
	/*
	Q_PROPERTY(ReportItemFrame* header READ header WRITE setHeader NOTIFY headerChanged)
	Q_PROPERTY(ReportItemDetail* detail READ detail WRITE setDetail NOTIFY detailChanged)
	Q_PROPERTY(ReportItemFrame* footer READ footer WRITE setFooter NOTIFY footerChanged)
	*/
private:
	typedef ReportItemFrame Super;
public:
	ReportItemBand(ReportItem *parent = nullptr);
	~ReportItemBand() Q_DECL_OVERRIDE;
public:
	QF_PROPERTY_IMPL(QString, d, D, ataSource)
	QF_PROPERTY_IMPL(qf::core::utils::TreeTable, d, D, ata)
	QF_PROPERTY_BOOL_IMPL(h, H, eaderOnBreak)
	/*
	QF_PROPERTY_OBJECT_IMPL(ReportItemFrame*, h, H, eader)
	QF_PROPERTY_OBJECT_IMPL(ReportItemDetail*, d, D, etail)
	QF_PROPERTY_OBJECT_IMPL(ReportItemFrame*, f, F, ooter)
	*/
public:
	virtual PrintResult printMetaPaint(QPaintDevice *paint_device, ReportItemMetaPaint *out, const Rect &bounding_rect);

	virtual  ReportItemBand* toBand()  {return this;}
	ReportItemDetail* detail();

	virtual qf::core::utils::TreeTable dataTable();
	virtual void resetIndexToPrintRecursively(bool including_para_texts);
protected:
	qf::core::utils::TreeTable f_dataTable;
	bool dataTableLoaded;
};
#endif

}}}

#endif // QF_GUI_REPORTS_REPORTITEMBAND_H
