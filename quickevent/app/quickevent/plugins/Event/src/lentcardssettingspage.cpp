#include "lentcardssettingspage.h"
#include "ui_lentcardssettingspage.h"

#include <qf/core/model/sqltablemodel.h>

namespace qfm = qf::core::model;
namespace qfs = qf::core::sql;

LentCardsSettingsPage::LentCardsSettingsPage(QWidget *parent)
	: Super(parent)
	, ui(new Ui::LentCardsSettingsPage)
{
	m_caption = tr("Cards to rent");
	ui->setupUi(this);

	ui->tblCardsTB->setTableView(ui->tblCards);
	{
		qfm::SqlTableModel *m = new qfm::SqlTableModel(this);
		m->addColumn("siId", tr("SI"));
		m->addColumn("ignored", tr("Ignored"));
		m->addColumn("note", tr("Note"));
		ui->tblCards->setTableModel(m);
		m_tableModel = m;
	}
}

LentCardsSettingsPage::~LentCardsSettingsPage()
{
	delete ui;
}

void LentCardsSettingsPage::load()
{
	qfs::QueryBuilder qb;
	qb.select2("lentcards", "*")
			.from("lentcards")
			.orderBy("siid");//.limit(10);
	m_tableModel->setQueryBuilder(qb, false);
	m_tableModel->reload();
}

void LentCardsSettingsPage::save()
{

}
