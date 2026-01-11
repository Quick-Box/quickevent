#pragma once

#include "sqltablemodel.h"

#include <qf/gui/model/sqldatadocument.h>

namespace qx {

class SqlDataDocument : public qf::gui::model::SqlDataDocument
{
	Q_OBJECT

	using Super = qf::gui::model::SqlDataDocument;
public:
	explicit SqlDataDocument(QObject *parent = nullptr);
protected:
	::qx::SqlTableModel* createModel(QObject *parent) override;
};

} // namespace qx

