#pragma once

#include "datadocument.h"
#include "sqltablemodel.h"

namespace qf::gui::model {

class QFGUI_DECL_EXPORT SqlDataDocument : public DataDocument
{
	Q_OBJECT
public:
	SqlDataDocument(QObject *parent = nullptr);
	~SqlDataDocument() Q_DECL_OVERRIDE;
private:
	typedef DataDocument Super;
public:
	SqlTableModel* sqlModel();

	qf::core::sql::QueryBuilder queryBuilder();
	void setQueryBuilder(const qf::core::sql::QueryBuilder &qb);
protected:
	SqlTableModel* createModel(QObject *parent) override;

	///! load model persistent storage via model
	bool loadData() Q_DECL_OVERRIDE;
	/*
	///! save dirty data to persistent storage via model
	virtual bool saveData();
	///! drop data in persistent storage via model
	virtual bool dropData();
	//! prepare loaded data to create document copy on next save() call
	virtual bool copyData();
	*/
};

}

