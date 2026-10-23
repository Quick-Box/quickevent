#include "combobox.h"

#include "datacontroller.h"

#include <qf/core/sql/dbenumcache.h>
#include <qf/core/sql/query.h>
#include <qf/core/sql/querybuilder.h>
#include <qf/core/log.h>

#include <QApplication>
#include <QLineEdit>
#include <QListView>
#include <QScreen>
#include <QScrollBar>
#include <QSet>

using namespace qf::gui;

//===============================================================
//              ComboBox
//===============================================================
ComboBox::ComboBox(QWidget *parent)
	: Super(parent), IDataWidget(this)
{
	connect(this, &ComboBox::currentTextChanged, this, &ComboBox::onCurrentTextChanged);
	connect(this, SIGNAL(currentIndexChanged(int)), this, SLOT(currentDataChanged_helper(int)));
	connect(this, SIGNAL(activated(int)), this, SLOT(currentDataActivated_helper(int)));
}

ComboBox::~ComboBox()
= default;

void ComboBox::setCurrentData(const QVariant &val)
{
	QVariant old_val = currentData();
	if(old_val != val) {
		for(int i=0; i<count(); i++) {
			QVariant v = itemData(i);
			if(v == val) {
				setCurrentIndex(i);
				emit currentDataChanged(val);
				break;
			}
		}
	}
}

void ComboBox::insertItem(int index, const QString &text, const QVariant &user_data)
{
	Super::insertItem(index, text, user_data);
}

void ComboBox::setItems(const QVariantList &items)
{
	blockSignals(true);
	clear();
	for(const auto &v : items) {
		QVariantList vl = v.toList();
		if(vl.isEmpty()) {
			Super::addItem(v.toString());
		}
		else {
			Super::addItem(vl.value(0).toString(), vl.value(1));
		}
	}
	setCurrentIndex(-1);
	blockSignals(false);
}

QVariant ComboBox::dataValue()
{
	QVariant ret;
	if(isEditable() && !isValueRestrictedToItems()) {
		ret = lineEdit()->text();
	}
	else {
		ret = currentData();
	}
	return ret;
}

void ComboBox::setDataValue(const QVariant &val)
{
	qfLogFuncFrame();
	bool emit_change = false;
	{
		//QSignalBlocker(this); do not work, don't know why, using m_loadingState hack instead
		int old_ix = currentIndex();
		QString old_text;
		if(isEditable()) {
			old_text = lineEdit()->text();
		}
		loadItems();
		m_ignoreIndexChangedSignals = true;
		int ix = findData(val);
		setCurrentIndex(ix);
		emit_change = ix != old_ix;
		if(ix < 0 && isEditable() && !isValueRestrictedToItems()) {
			lineEdit()->setText(val.toString());
			emit_change = emit_change || (lineEdit()->text() != old_text);
		}
		m_ignoreIndexChangedSignals = false;
	}
	if(emit_change) {
		saveDataValue();
		emit dataValueChanged(val);
	}
}

void ComboBox::removeItems()
{
	Super::clear();
}

void ComboBox::loadItems(bool force)
{
	Q_UNUSED(force)
}

void ComboBox::setMultiColumnPopup(bool b)
{
	if(b == m_multiColumnPopup) {
		return;
	}
	m_multiColumnPopup = b;
	auto *lv = new QListView();
	if(b) {
		lv->setFlow(QListView::TopToBottom);
		lv->setWrapping(true);
		lv->setResizeMode(QListView::Adjust);
	}
	setView(lv);
	if(b) {
		// set multi-column geometry before the popup window appears on screen
		lv->window()->installEventFilter(this);
	}
}

void ComboBox::showPopup()
{
	if(!m_multiColumnPopup) {
		Super::showPopup();
		return;
	}
	// no roll-out animation, it would show the popup in its single column size first
	bool animate = QApplication::isEffectEnabled(Qt::UI_AnimateCombo);
	QApplication::setEffectEnabled(Qt::UI_AnimateCombo, false);
	Super::showPopup();
	QApplication::setEffectEnabled(Qt::UI_AnimateCombo, animate);
	// Qt can adjust geometry after the popup is shown, apply ours again
	updateMultiColumnPopupGeometry();
}

bool ComboBox::eventFilter(QObject *watched, QEvent *event)
{
	if(m_multiColumnPopup && event->type() == QEvent::Show && watched == view()->window()) {
		updateMultiColumnPopupGeometry();
	}
	return Super::eventFilter(watched, event);
}

void ComboBox::updateMultiColumnPopupGeometry()
{
	if(count() == 0) {
		return;
	}
	static constexpr int max_rows = 20;
	QAbstractItemView *v = view();
	QWidget *popup = v->window();
	QRect avail = screen()->availableGeometry();
	int row_h = v->sizeHintForRow(0);
	int col_w = 0;
	for(int i = 0; i < count(); i++) {
		col_w = qMax(col_w, v->sizeHintForIndex(model()->index(i, modelColumn())).width());
	}
	if(row_h <= 0 || col_w <= 0) {
		return;
	}
	// popup frame + container margins around the view's viewport, without currently shown scroll bars
	QScrollBar *hsb = v->horizontalScrollBar();
	QScrollBar *vsb = v->verticalScrollBar();
	int extra_w = popup->width() - v->viewport()->width() - (vsb->isVisible() ? vsb->width() : 0);
	int extra_h = popup->height() - v->viewport()->height() - (hsb->isVisible() ? hsb->height() : 0);
	int rows = qBound(1, (avail.height() - extra_h) / row_h, max_rows);
	int cols = (count() + rows - 1) / rows;
	rows = (count() + cols - 1) / cols;
	int w = cols * col_w + extra_w;
	// +1: QListView wraps against QRect::bottom(), which is height - 1
	int h = rows * row_h + extra_h + 1;
	if(w > avail.width()) {
		// does not fit even in columns, horizontal scroll bar will appear
		w = avail.width();
		h += hsb->sizeHint().height();
	}
	w = qMax(w, width());
	QPoint pos = mapToGlobal(QPoint(0, height()));
	if(pos.y() + h > avail.bottom()) {
		pos.setY(mapToGlobal(QPoint(0, 0)).y() - h);
	}
	pos.setX(qBound(avail.left(), pos.x(), avail.right() - w));
	pos.setY(qBound(avail.top(), pos.y(), avail.bottom() - h));
	popup->setGeometry(QRect(pos, QSize(w, h)));
	v->scrollTo(v->currentIndex());
}

void ComboBox::onCurrentTextChanged(const QString &txt)
{
	if(!m_ignoreIndexChangedSignals) {
		qfLogFuncFrame() << txt;
		saveDataValue();
		emit dataValueChanged(dataValue());
	}
}

void ComboBox::currentDataChanged_helper(int ix)
{
	emit currentDataChanged(itemData(ix));
}

void ComboBox::currentDataActivated_helper(int ix)
{
	emit currentDataActivated(itemData(ix));
}

//===============================================================
//              ForeignKeyComboBox
//===============================================================
ForeignKeyComboBox::ForeignKeyComboBox(QWidget *parent)
	: Super(parent)
{
}

void ForeignKeyComboBox::removeItems()
{
	Super::removeItems();
	m_itemsLoaded = false;
}

void ForeignKeyComboBox::loadItems(bool force)
{
	static QString referencedTablePlaceHolder = QStringLiteral("{{referencedTable}}");
	static QString referencedFieldPlaceHolder = QStringLiteral("{{referencedField}}");
	static QString referencedCaptionFieldPlaceHolder = QStringLiteral("{{captionField}}");
	if(force)
		removeItems();
	if(!m_itemsLoaded) do {
		QString connection_name;
		qfLogFuncFrame() << this << "data controler:" << m_dataController;
		if(!m_dataController) {
			connection_name = QLatin1String(QSqlDatabase::defaultConnection);
			qfDebug() << "Data controller is NULL, using default connection name:" << connection_name;
		}
		m_ignoreIndexChangedSignals = true;
		{
			QString tblname = referencedTable();
			QString fldname = referencedField();
			QString capfldname = referencedCaptionField();
			if(capfldname.isEmpty())
				capfldname = fldname;
			QString query_str = queryString();
			if(query_str.isEmpty()) {
				if(!tblname.isEmpty() && !fldname.isEmpty()) {
					qf::core::sql::QueryBuilder qb;
					qb.select2(tblname, fldname);
					qb.from(tblname);
					if(capfldname != fldname) {
						qb.select(capfldname);
						qb.orderBy(capfldname);
					}
					else {
						qb.orderBy(fldname);
					}
					query_str = qb.toString();
				}
			}
			else {
				query_str = query_str.replace(referencedTablePlaceHolder, tblname);
				query_str = query_str.replace(referencedFieldPlaceHolder, fldname);
				query_str = query_str.replace(referencedCaptionFieldPlaceHolder, capfldname);
			}
			query_str = query_str.trimmed();
			if(!query_str.isEmpty()) {
				qf::core::sql::Query q(connection_name);
				qfDebug() << "\t query_str:" << query_str;
				QSet<QString> field_captions;
				QString caption_format = itemCaptionFormat();
				if(!caption_format.isEmpty()) {
					qfDebug() << "\t itemCaptionFormat:" << caption_format;
					if(caption_format.contains(referencedTablePlaceHolder, Qt::CaseInsensitive))
						caption_format.replace(referencedTablePlaceHolder, tblname, Qt::CaseInsensitive);
					if(caption_format.contains(referencedFieldPlaceHolder, Qt::CaseInsensitive))
						caption_format.replace(referencedFieldPlaceHolder, "{{" + fldname + "}}", Qt::CaseInsensitive);
					if(caption_format.contains(referencedCaptionFieldPlaceHolder, Qt::CaseInsensitive))
						caption_format.replace(referencedCaptionFieldPlaceHolder, "{{" + capfldname + "}}", Qt::CaseInsensitive);
					field_captions = qf::core::Utils::findCaptions(caption_format);
				}
				qfDebug() << "\t capname:" << capfldname;
				q.exec(query_str);
				while(q.next()) {
					QString format_str = caption_format;
					for (const auto &fld : field_captions) {
						qfDebug() << "\t replacing caption field:" << fld;
						QVariant fld_val = q.value(fld);
						format_str = qf::core::Utils::replaceCaptions(format_str, fld, fld_val);
					}
					QVariant id = q.value(fldname);
					addItem(format_str, id);
				}
			}
		}
		m_itemsLoaded = true;
		qfDebug() << "\t item count:" << count();
		m_ignoreIndexChangedSignals = false;
	} while(false);
}

//===============================================================
//              DbEnumComboBox
//===============================================================
DbEnumComboBox::DbEnumComboBox(QWidget *parent)
	: Super(parent)
{
}

void DbEnumComboBox::removeItems()
{
	Super::removeItems();
	m_itemsLoaded = false;
}

void DbEnumComboBox::loadItems(bool force)
{
	qfLogFuncFrame() << "force:" << force;
	if(force)
		removeItems();
	if(!m_itemsLoaded) {
		QString connection_name;
		qfLogFuncFrame() << this << "data controler:" << m_dataController;
		if(!m_dataController) {
			connection_name = QLatin1String(QSqlDatabase::defaultConnection);
			qfDebug() << "Data controller is NULL, using default connection name:" << connection_name;
		}
		m_ignoreIndexChangedSignals = true;
		{
			qf::core::sql::DbEnumCache& db_enum_cache = qf::core::sql::DbEnumCache::instanceForConnection(connection_name);
			Q_FOREACH(auto dbe, db_enum_cache.dbEnumsForGroup(groupName())) {
				QString cap = dbe.fillInPlaceholders(itemCaptionFormat());
				addItem(cap, dbe.groupId());
				QColor c = dbe.color();
				if(c.isValid()) {
					setItemData(count() - 1, c, Qt::BackgroundRole);
					setItemData(count() - 1, qf::gui::model::TableModel::contrastTextColor(c), Qt::ForegroundRole);
				}
			}
		}
		m_itemsLoaded = true;
		qfDebug() << "\t item count:" << count();
		m_ignoreIndexChangedSignals = false;
	};
}
