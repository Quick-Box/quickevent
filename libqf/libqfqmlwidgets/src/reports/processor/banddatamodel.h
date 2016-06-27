//
// Author: Frantisek Vacek <fanda.vacek@volny.cz>, (C) 2006, 2014
//
// Copyright: See COPYING file that comes with this distribution
//

#ifndef QF_QMLWIDGETS_REPORTS_BANDDTAMODEL_H
#define QF_QMLWIDGETS_REPORTS_BANDDTAMODEL_H

#include "../../qmlwidgetsglobal.h"

#include <qf/core/utils.h>
#include <qf/core/utils/treetable.h>

#include <QObject>

namespace qf {
namespace qmlwidgets {
namespace reports {

class QFQMLWIDGETS_DECL_EXPORT BandDataModel : public QObject
{
	Q_OBJECT
private:
	typedef QObject Super;
public:
	typedef Qt::ItemDataRole DataRole;
public:
	explicit BandDataModel(QObject *parent = 0);
	~BandDataModel() Q_DECL_OVERRIDE;

	QF_PROPERTY_BOOL_IMPL2(d, D, ataValid, false)
public:
	virtual int rowCount() = 0;
	virtual int columnCount() = 0;
	virtual QVariant tableData(const QString &key, DataRole role = Qt::DisplayRole) = 0;
	virtual QVariant headerData(int col_no, DataRole role = Qt::DisplayRole) = 0;
	virtual QVariant data(int row_no, const QString &col_name, DataRole role = Qt::DisplayRole) = 0;
	virtual QVariant data(int row_no, int col_no, DataRole role = Qt::DisplayRole) = 0;
	virtual QVariant table(int row_no, const QString &table_name);
	virtual QString dump() const {return QString();}

	Q_SLOT void invalidateData() {setDataValid(false);}
public:
	static BandDataModel* createFromData(const QVariant &data, QObject *parent = nullptr);
};

class TreeTableBandDataModel : public BandDataModel
{
	Q_OBJECT
private:
	typedef BandDataModel Super;
public:
	explicit TreeTableBandDataModel(QObject *parent = 0);
public:
	int rowCount() Q_DECL_OVERRIDE;
	int columnCount() Q_DECL_OVERRIDE;
	QVariant tableData(const QString &key, DataRole role = Qt::DisplayRole) Q_DECL_OVERRIDE;
	QVariant headerData(int col_no, DataRole role = Qt::DisplayRole) Q_DECL_OVERRIDE;
	QVariant data(int row_no, int col_no, DataRole role = Qt::DisplayRole) Q_DECL_OVERRIDE;
	QVariant data(int row_no, const QString &col_name, DataRole role = Qt::DisplayRole) Q_DECL_OVERRIDE;
	QVariant table(int row_no, const QString &table_name) Q_DECL_OVERRIDE;
	QString dump() const Q_DECL_OVERRIDE;

	const qf::core::utils::TreeTable& treeTable() const;
	void setTreeTable(const qf::core::utils::TreeTable &tree_table);
private:
	qf::core::utils::TreeTable m_treeTable;
};

}}}

#endif // QF_QMLWIDGETS_REPORTS_BANDDTAMODEL_H
