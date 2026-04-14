#ifndef COMPETITORS_COMPETITORDOCUMENT_H
#define COMPETITORS_COMPETITORDOCUMENT_H

#include "src/qx/sqldatadocument.h"

#include <QVector>

namespace Competitors {

class CompetitorDocument : public qx::SqlDataDocument
{
	Q_OBJECT
private:
	typedef qx::SqlDataDocument Super;
public:
	CompetitorDocument(QObject *parent = nullptr);

	void setEmitDbEventsOnSave(bool b) {m_isEmitDbEventsOnSave = b;}

	void setSiid(const QVariant &siid);
	QVariant siid() const;
	const QVector<int>& runsIds() const {return m_runsIds;}
protected:
	bool loadData() override;
	bool saveData() override;
	bool dropData() override;
private:
	bool m_isEmitDbEventsOnSave = true;
	QVector<int> m_runsIds;
};

}

#endif // COMPETITORS_COMPETITORDOCUMENT_H
