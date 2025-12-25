#pragma once

#include <quickevent/gui/og/sqltablemodel.h>

namespace qx {

class SqlTableModel : public quickevent::gui::og::SqlTableModel
{
	Q_OBJECT

	using Super = quickevent::gui::og::SqlTableModel;
public:
	explicit SqlTableModel(QObject *parent = nullptr);
};

} // namespace qx

