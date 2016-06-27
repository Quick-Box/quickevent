#ifndef QF_CORE_SQL_TRANSACTION_H
#define QF_CORE_SQL_TRANSACTION_H

#include "../core/coreglobal.h"
#include "connection.h"

namespace qf {
namespace core {
namespace sql {

class QFCORE_DECL_EXPORT Transaction
{
public:
	Transaction(const Connection &conn = Connection());
	virtual ~Transaction();

	void commit();
	void rollback();

	const Connection& connection() const {return m_connection;}
protected:
	Connection m_connection;
	bool m_finished = false;
};

}}}

#endif // QF_CORE_SQL_TRANSACTION_H
