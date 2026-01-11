#pragma once

#include "quickeventguiglobal.h"

#include <qf/gui/framework/ipersistentsettings.h>

#include <qf/core/utils.h>

#include <QDialog>
#include <QVariantMap>

namespace quickevent {
namespace gui {

namespace Ui {
class ReportOptionsDialog;
}

class QUICKEVENTGUI_DECL_EXPORT ReportOptionsDialog : public QDialog, public qf::gui::framework::IPersistentSettings
{
	Q_OBJECT

	Q_PROPERTY(QString persistentSettingsId READ persistentSettingsId WRITE setPersistentSettingsId NOTIFY persistentSettingsIdChanged)
	Q_PROPERTY(bool classFilterVisible READ isClassFilterVisible WRITE setClassFilterVisible NOTIFY classFilterVisibleChanged)
	Q_PROPERTY(bool startListOptionsVisible READ isStartListOptionsVisible WRITE setStartListOptionsVisible NOTIFY startListOptionsVisibleChanged)
	Q_PROPERTY(bool startersOptionsVisible READ isStartersOptionsVisible WRITE setStartersOptionsVisible NOTIFY startersOptionsVisibleChanged)
	Q_PROPERTY(bool vacantsVisible READ isVacantsVisible WRITE setVacantsVisible NOTIFY vacantsVisibleChanged)
	Q_PROPERTY(bool stagesOptionVisible READ isStagesOptionVisible WRITE setStagesOptionVisible NOTIFY stagesOptionVisibleChanged)
	//Q_PROPERTY(int stagesCount READ stagesCount WRITE setStagesCount NOTIFY stagesCountChanged)
	Q_PROPERTY(bool pageLayoutVisible READ isPageLayoutVisible WRITE setPageLayoutVisible NOTIFY pageLayoutVisibleChanged)
	Q_PROPERTY(bool columnCountEnable READ isColumnCountEnable WRITE setColumnCountEnable NOTIFY columnCountEnableChanged)
	Q_PROPERTY(bool resultOptionsVisible READ isResultOptionsVisible WRITE setResultOptionsVisible NOTIFY resultOptionsVisibleChanged)
	Q_PROPERTY(bool startTimeFormatVisible READ isStartTimeFormatVisible WRITE setStartTimeFormatVisible NOTIFY startTimeFormatVisibleChanged)
	Q_PROPERTY(bool startlistOrderFirstByVisible READ isStartlistOrderFirstByVisible WRITE setStartlistOrderFirstByVisible NOTIFY startlistOrderFirstByVisibleChanged)
	//Q_PROPERTY(bool classStartSelectionVisible READ isClassStartSelectionVisible WRITE setClassStartSelectionVisible NOTIFY classStartSelectionVisibleChanged)
	Q_PROPERTY(bool relayOptionsVisible READ isRelayOptionsVisible WRITE setRelayOptionsVisible NOTIFY relayOptionsVisibleChanged)

	QF_PROPERTY_BOOL_IMPL2(c, C, lassFilterVisible, true)
	QF_PROPERTY_BOOL_IMPL2(s, S, tartListOptionsVisible, false)
	QF_PROPERTY_BOOL_IMPL2(s, S, tartersOptionsVisible, false)
	QF_PROPERTY_BOOL_IMPL2(v, V, acantsVisible, true)
	QF_PROPERTY_BOOL_IMPL2(s, S, tagesOptionVisible, false)
	QF_PROPERTY_BOOL_IMPL2(l, L, egsOptionVisible, false)
	QF_PROPERTY_BOOL_IMPL2(p, P, ageLayoutVisible, true)
	QF_PROPERTY_BOOL_IMPL2(c, C, olumnCountEnable, true)
	QF_PROPERTY_BOOL_IMPL2(r, R, esultOptionsVisible, false)
	QF_PROPERTY_BOOL_IMPL2(s, S, tartTimeFormatVisible, false)
	QF_PROPERTY_BOOL_IMPL2(s, S, tartlistOrderFirstByVisible, false)
	QF_PROPERTY_BOOL_IMPL2(r, R, elayOptionsVisible, false)
private:
	using Super = QDialog;
public:
	enum class BreakType : int {None = 0, Column, Page};
	enum class FilterType : int {WildCard = 0, RegExp, ClassName};
	enum class StartTimeFormat : int {RelativeToClassStart = 0, DayTime};
	enum class StartlistOrderFirstBy : int { ClassName = 0, StartTime, Names };

	class Options : public QVariantMap
	{
		QF_VARIANTMAP_FIELD2(int, s, setS, tartersOptionsLineSpacing, 0)
		QF_VARIANTMAP_FIELD2(int, b, setB, reakType, 0)
		QF_VARIANTMAP_FIELD2(QString, c, setC, olumns, "%,%")
		QF_VARIANTMAP_FIELD2(int, p, setP, ageWidth, 210)
		QF_VARIANTMAP_FIELD2(int, p, setP, ageHeight, 297)
		QF_VARIANTMAP_FIELD2(int, c, setC, olumnCount, 2)
		QF_VARIANTMAP_FIELD2(int, h, setH, orizontalMargin, 10)
		QF_VARIANTMAP_FIELD2(int, v, setV, erticalMargin, 5)
		QF_VARIANTMAP_FIELD(QString, c, setC, lassFilter)
		QF_VARIANTMAP_FIELD2(int, c, setC, lassFilterType, 0)
		QF_VARIANTMAP_FIELD(bool, is, set, UseClassFilter)
		QF_VARIANTMAP_FIELD(bool, is, set, InvertClassFilter)
		QF_VARIANTMAP_FIELD(bool, is, set, StartListPrintVacants)
		QF_VARIANTMAP_FIELD(bool, is, set, StartListPrintStartNumbers)
		QF_VARIANTMAP_FIELD2(int, s, setS, tagesCount, 1)
		QF_VARIANTMAP_FIELD2(int, l, setL, egsCount, 1)
		QF_VARIANTMAP_FIELD2(int, r, setR, esultNumPlaces, 9999)
		QF_VARIANTMAP_FIELD2(bool, isR, setR, esultExcludeDisq, false)
		QF_VARIANTMAP_FIELD2(int, s, setS, tartTimeFormat, 0)
		QF_VARIANTMAP_FIELD2(int, s, setS, tartlistOrderFirstBy, 0)
		QF_VARIANTMAP_FIELD2(int, s, setS, tartNumber, 0)
		QF_VARIANTMAP_FIELD2(bool, isR, setR, elayShowLegsDetails, true)
		public:
			Options(const QVariantMap &o = QVariantMap()) : QVariantMap(o) {}
	};
public:
	explicit ReportOptionsDialog(QWidget *parent = nullptr);
	~ReportOptionsDialog() override;

	int exec() override;

	void setStartListPrintVacantsVisible(bool b);
	void setStartListForRelays();

	QString persistentSettingsPath() override;
	bool setPersistentSettingsId(const QString &id) override;
	Q_SIGNAL void persistentSettingsIdChanged(const QString &id);

	void setOptions(const Options &options);
	Options options() const;
	Q_INVOKABLE QVariantMap optionsMap() const {return options();}
	static Options savedOptions(const QString &persistent_settings_id = QString());

	Q_INVOKABLE QVariantMap reportProperties() const;

	void loadPersistentSettings(const Options &default_options);
	Q_SLOT void loadPersistentSettings();
	Q_SLOT void savePersistentSettings();
	Q_SLOT void resetPersistentSettings();

	void setClassNamesFilter(const QStringList &class_names);

	int stagesCount() const;
	void setStagesCount(int n);

	bool resultExcludeDisq() const;
	void setResultExcludeDisq(bool b);

	StartTimeFormat startTimeFormat() const;
	StartlistOrderFirstBy startlistOrderFirstBy() const;
	BreakType breakType() const;
	Q_INVOKABLE bool isStartListPrintVacants() const;
	Q_INVOKABLE bool isStartListPrintStartNumbers() const;
	Q_INVOKABLE bool isBreakAfterEachClass() const {return breakType() != BreakType::None;}
	Q_INVOKABLE bool isColumnBreak() const {return breakType() == BreakType::Column;}
	Q_INVOKABLE int resultNumPlaces() const;
	Q_INVOKABLE QString sqlWhereExpression(const int stage_id = 1) const;
	static QString sqlWhereExpression(const Options &opts, const int stage_id);
	static QString getClassesForStartNumber(const int number, const int stage_id);
protected:
	//void showEvent(QShowEvent *event) override;
private:
	Ui::ReportOptionsDialog *ui;
};

}}
