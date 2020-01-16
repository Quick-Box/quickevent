#ifndef RELAYS_RELAYDOCUMENT_H
#define RELAYS_RELAYDOCUMENT_H

#include "relayspluginglobal.h"

#include <qf/core/model/sqldatadocument.h>

namespace Relays {

class RELAYSPLUGIN_DECL_EXPORT  RelayDocument : public qf::core::model::SqlDataDocument
{
	Q_OBJECT
private:
	typedef qf::core::model::SqlDataDocument Super;
public:
	 RelayDocument(QObject *parent = nullptr);
protected:
	bool saveData() Q_DECL_OVERRIDE;
	bool dropData() Q_DECL_OVERRIDE;
};

}

#endif // RELAYS_RELAYDOCUMENT_H
