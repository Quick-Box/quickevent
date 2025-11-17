#include "sqltablemodel.h"
#include "sqlapi.h"

namespace qx {

SqlTableModel::SqlTableModel(QObject *parent)
	: Super{parent}
{
	auto *sql_api = SqlApi::instance();
	connect(this, &qf::gui::model::SqlTableModel::qxRecChng, sql_api, &SqlApi::emitRecChng);
}

} // namespace qx
