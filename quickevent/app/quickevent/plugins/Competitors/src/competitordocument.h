#pragma once

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

	void setEmitDbEventsOnSave(bool b) {m_isEmitDbEventsOnSave = b;}

	void setSiid(const QVariant &siid);
	QVariant siid() const;
	const QVector<int>& runsIds() const {return m_runsIds;}

	static QList<int> possibleStartTimesMs(int run_id);
protected:
	bool loadData() override;
	bool saveData() override;
	bool dropData() override;
private:
	bool m_isEmitDbEventsOnSave = true;
	QVector<int> m_runsIds;
};

}

