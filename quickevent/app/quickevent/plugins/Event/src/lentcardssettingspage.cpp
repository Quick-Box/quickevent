#include "lentcardssettingspage.h"
#include "ui_lentcardssettingspage.h"

#include <qf/gui/model/sqltablemodel.h>

namespace qfm = qf::gui::model;
namespace qfs = qf::core::sql;

LentCardsSettingsPage::LentCardsSettingsPage(QWidget *parent)
	: Super(parent)
	, ui(new Ui::LentCardsSettingsPage)
{
	m_caption = tr("Cards to rent");
	m_requiresOpenEvent = true;
	ui->setupUi(this);

	ui->tblCardsTB->setTableView(ui->tblCards);
	{
		auto *m = new qfm::SqlTableModel(this);
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
