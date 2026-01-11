#include "sqldatadocument.h"


namespace qx {

SqlDataDocument::SqlDataDocument(QObject *parent)
	: Super{parent}
{

}

SqlTableModel *SqlDataDocument::createModel(QObject *parent)
{
	return new ::qx::SqlTableModel(parent);
}

} // namespace qx
