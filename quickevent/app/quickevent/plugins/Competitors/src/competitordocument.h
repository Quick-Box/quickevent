#ifndef COMPETITORS_COMPETITORDOCUMENT_H
#define COMPETITORS_COMPETITORDOCUMENT_H

#include <qf/gui/model/sqldatadocument.h>

#include <QVector>

namespace Competitors {

class CompetitorDocument : public qf::gui::model::SqlDataDocument
{
	Q_OBJECT
private:
	typedef qf::gui::model::SqlDataDocument Super;
public:
	CompetitorDocument(QObject *parent = nullptr);

	//bool isSaveSiidToRuns() const {return m_saveSiidToRuns;}
	void setEmitDbEventsOnSave(bool b) {m_isEmitDbEventsOnSave = b;}

	//void setSiid(const QVariant &siid, bool save_siid_to_runs);
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
