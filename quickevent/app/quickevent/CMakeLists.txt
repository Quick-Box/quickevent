add_executable(quickevent
	plugins/CardReader/src/cardchecker.cpp
	plugins/CardReader/src/cardcheckerclassiccpp.cpp
	plugins/CardReader/src/cardcheckerfreeordercpp.cpp
	plugins/CardReader/src/cardreaderplugin.cpp
	plugins/CardReader/src/cardreaderwidget.cpp
	plugins/CardReader/src/cardreaderwidget.ui
	plugins/CardReader/src/services/mqttpunches.cpp
	plugins/CardReader/src/services/mqttpuncheswidget.cpp
	plugins/CardReader/src/services/mqttpuncheswidget.ui
	plugins/CardReader/src/services/racomclient.cpp
	plugins/CardReader/src/services/racomclientwidget.cpp
	plugins/CardReader/src/services/racomclientwidget.ui
	plugins/CardReader/src/cardreadersettingspage.cpp
	plugins/CardReader/src/cardreadersettingspage.ui
	plugins/CardReader/src/cardreadersettings.cpp
	plugins/CardReader/src/services/qropunch.cpp
	plugins/CardReader/src/services/qropunchwidget.cpp
	plugins/CardReader/src/services/qropunchwidget.ui

	plugins/Classes/src/classdefdocument.cpp
	plugins/Classes/src/classdefwidget.cpp
	plugins/Classes/src/classdefwidget.ui
	plugins/Classes/src/classdocument.cpp
	plugins/Classes/src/classesplugin.cpp
	plugins/Classes/src/classestableview.cpp
	plugins/Classes/src/classeswidget.cpp
	plugins/Classes/src/classeswidget.ui
	plugins/Classes/src/drawing/classitem.cpp
	plugins/Classes/src/drawing/drawingganttwidget.cpp
	plugins/Classes/src/drawing/drawingganttwidget.ui
	plugins/Classes/src/drawing/ganttitem.cpp
	plugins/Classes/src/drawing/ganttruler.cpp
	plugins/Classes/src/drawing/ganttscene.cpp
	plugins/Classes/src/drawing/ganttview.cpp
	plugins/Classes/src/drawing/iganttitem.cpp
	plugins/Classes/src/drawing/startslotheader.cpp
	plugins/Classes/src/drawing/startslotitem.cpp
	plugins/Classes/src/editcodeswidget.cpp
	plugins/Classes/src/editcodeswidget.ui
	plugins/Classes/src/editcoursecodeswidget.cpp
	plugins/Classes/src/editcoursecodeswidget.ui
	plugins/Classes/src/editcourseswidget.cpp
	plugins/Classes/src/editcourseswidget.ui
	plugins/Classes/src/importcoursedef.cpp
	plugins/Classes/src/courseitemdelegate.h plugins/Classes/src/courseitemdelegate.cpp

	plugins/Competitors/src/competitordocument.cpp
	plugins/Competitors/src/competitorwidget.cpp
	plugins/Competitors/src/competitorwidget.ui

	plugins/Core/src/coreplugin.cpp
	plugins/Core/src/reportssettings.cpp
	plugins/Core/src/settings.cpp
	plugins/Core/src/widgets/reportssettingspage.cpp
	plugins/Core/src/widgets/reportssettingspage.ui
	plugins/Core/src/widgets/appstatusbar.cpp
	plugins/Core/src/widgets/settingspage.cpp
	plugins/Core/src/widgets/settingsdialog.cpp
	plugins/Core/src/widgets/settingsdialog.ui

	plugins/Event/src/connectdbdialogwidget.cpp
	plugins/Event/src/connectdbdialogwidget.ui
	plugins/Event/src/connectionsettings.cpp
	plugins/Event/src/dbschema.cpp
	plugins/Event/src/eventconfig.cpp
	plugins/Event/src/eventdialogwidget.cpp
	plugins/Event/src/eventdialogwidget.ui
	plugins/Event/src/eventplugin.cpp
	plugins/Event/src/stage.cpp
	plugins/Event/src/stagedocument.cpp
	plugins/Event/src/stagewidget.cpp
	plugins/Event/src/stagewidget.ui
	plugins/Event/src/findregistrationedit.cpp
	plugins/Event/src/lentcardssettingspage.cpp
	plugins/Event/src/lentcardssettingspage.ui
	plugins/Event/src/registrationswidget.cpp
	plugins/Event/src/registrationswidget.ui
	plugins/Event/src/services/emmaclient.cpp
	plugins/Event/src/services/emmaclientwidget.cpp
	plugins/Event/src/services/emmaclientwidget.ui
	plugins/Event/src/services/oresultsclient.cpp
	plugins/Event/src/services/oresultsclientwidget.cpp
	plugins/Event/src/services/oresultsclientwidget.ui
	plugins/Event/src/services/ofeed/ofeedclient.cpp
	plugins/Event/src/services/ofeed/ofeedclientwidget.cpp
	plugins/Event/src/services/ofeed/ofeedclientwidget.ui
	plugins/Event/src/services/service.cpp
	plugins/Event/src/services/serviceswidget.cpp
	plugins/Event/src/services/servicewidget.cpp
	plugins/Event/src/services/servicewidget.ui
	plugins/Event/src/services/qx/qxclientservice.cpp
	plugins/Event/src/services/qx/qxclientservice.h
	plugins/Event/src/services/qx/qxclientservicewidget.cpp
	plugins/Event/src/services/qx/qxclientservicewidget.h
	plugins/Event/src/services/qx/qxclientservicewidget.ui
	plugins/Event/src/services/qx/qxlateregistrationswidget.h
	plugins/Event/src/services/qx/qxlateregistrationswidget.cpp
	plugins/Event/src/services/qx/qxlateregistrationswidget.ui
	plugins/Event/src/services/qx/runchangedialog.h plugins/Event/src/services/qx/runchangedialog.cpp plugins/Event/src/services/qx/runchangedialog.ui
	plugins/Event/src/services/qx/runchange.h plugins/Event/src/services/qx/runchange.cpp

	plugins/Oris/src/chooseoriseventdialog.cpp
	plugins/Oris/src/chooseoriseventdialog.ui
	plugins/Oris/src/orisimporter.cpp
	plugins/Oris/src/orisplugin.cpp
	plugins/Oris/src/txtimporter.cpp
	plugins/Oris/src/xmlimporter.cpp

	plugins/Receipts/src/receiptsplugin.cpp
	plugins/Receipts/src/receiptsprinter.cpp
	plugins/Receipts/src/receiptsprinteroptions.cpp
	plugins/Receipts/src/receiptsprinteroptionsdialog.cpp
	plugins/Receipts/src/receiptsprinteroptionsdialog.ui
	plugins/Receipts/src/receiptssettings.cpp
	plugins/Receipts/src/receiptssettingspage.cpp
	plugins/Receipts/src/receiptssettingspage.ui
	plugins/Receipts/src/thirdparty/qrcodegen.cpp
	plugins/Receipts/src/receiptswidget.cpp
	plugins/Receipts/src/receiptswidget.ui

	plugins/Relays/src/addlegdialogwidget.cpp
	plugins/Relays/src/addlegdialogwidget.ui
	plugins/Relays/src/relaydocument.cpp
	plugins/Relays/src/relaysplugin.cpp
	plugins/Relays/src/relayswidget.cpp
	plugins/Relays/src/relayswidget.ui
	plugins/Relays/src/relaywidget.cpp
	plugins/Relays/src/relaywidget.ui
	plugins/Relays/src/relaystableitemdelegate.cpp

	plugins/Runs/src/cardflagsdialog.cpp
	plugins/Runs/src/cardflagsdialog.ui
	plugins/Runs/src/eventstatisticsoptions.cpp
	plugins/Runs/src/eventstatisticsoptions.ui
	plugins/Runs/src/eventstatisticswidget.cpp
	plugins/Runs/src/eventstatisticswidget.ui
	plugins/Runs/src/findrunneredit.cpp
	plugins/Runs/src/findrunnerwidget.cpp
	plugins/Runs/src/findrunnerwidget.ui
	plugins/Runs/src/nstagesreportoptionsdialog.cpp
	plugins/Runs/src/nstagesreportoptionsdialog.ui
	plugins/Runs/src/printawardsoptionsdialogwidget.cpp
	plugins/Runs/src/printawardsoptionsdialogwidget.ui
	plugins/Runs/src/runflagsdialog.cpp
	plugins/Runs/src/runflagsdialog.ui
	plugins/Runs/src/runsplugin.cpp
	plugins/Runs/src/runstabledialogwidget.cpp
	plugins/Runs/src/runstabledialogwidget.ui
	plugins/Runs/src/runstableitemdelegate.cpp
	plugins/Runs/src/runstablemodel.cpp
	plugins/Runs/src/runstablewidget.cpp
	plugins/Runs/src/runstablewidget.ui
	plugins/Runs/src/runswidget.cpp
	plugins/Runs/src/runswidget.ui
	plugins/Runs/src/services/resultsexporter.cpp
	plugins/Runs/src/services/resultsexporterwidget.cpp
	plugins/Runs/src/services/resultsexporterwidget.ui

	plugins/Speaker/src/codeclassresultswidget.cpp
	plugins/Speaker/src/codeclassresultswidget.ui
	plugins/Speaker/src/punchestableview.cpp
	plugins/Speaker/src/speakerplugin.cpp
	plugins/Speaker/src/speakerwidget.cpp
	plugins/Speaker/src/speakerwidget.ui

	plugins/shared/shared.qrc
	plugins/Receipts/Receipts.qrc
	plugins/Event/Event.qrc
	plugins/Competitors/Competitors.qrc
	plugins/Speaker/Speaker.qrc
	plugins/Classes/Classes.qrc
	plugins/Relays/Relays.qrc
	plugins/Runs/Runs.qrc
	plugins/CardReader/CardReader.qrc

	src/appclioptions.cpp
	src/application.cpp
	src/loggerwidget.cpp
	src/main.cpp
	src/mainwindow.cpp
	src/partwidget.cpp

	style/style.qrc
	images/images.qrc

	quickevent.rc

	plugins/Classes/qml/reports/table.qml

	plugins/Event/qml/DbSchema.qml
	plugins/Event/qml/sql/def/Boolean.qml
	plugins/Event/qml/sql/def/Date.qml
	plugins/Event/qml/sql/def/DateTime.qml
	plugins/Event/qml/sql/def/Enum.qml
	plugins/Event/qml/sql/def/Field.qml
	plugins/Event/qml/sql/def/ForeignKeyReference.qml
	plugins/Event/qml/sql/def/Index.qml
	plugins/Event/qml/sql/def/Insert.qml
	plugins/Event/qml/sql/def/Int.qml
	plugins/Event/qml/sql/def/Real.qml
	plugins/Event/qml/sql/def/Schema.qml
	plugins/Event/qml/sql/def/Serial.qml
	plugins/Event/qml/sql/def/String.qml
	plugins/Event/qml/sql/def/Table.qml
	plugins/Event/qml/sql/def/Time.qml
	plugins/Event/qml/sql/def/private/FieldType.qml

	plugins/Receipts/qml/reports/error.qml
	plugins/Receipts/qml/reports/errorlong.qml
	plugins/Receipts/qml/reports/sicard.qml
	plugins/Receipts/qml/reports/receipts/Classic.qml
	plugins/Receipts/qml/reports/receipts/ClassicLottery.qml
	plugins/Receipts/qml/reports/receipts/Default.qml
	plugins/Receipts/qml/reports/receipts/private/LotteryTicket.qml

	plugins/Relays/qml/reports/results.qml
	plugins/Relays/qml/reports/results_condensed.qml
	plugins/Relays/qml/reports/startList_classes.qml
	plugins/Relays/qml/reports/startList_clubs.qml
	plugins/Relays/qml/reports/startList_classes_condensed.qml
	plugins/Relays/qml/reports/startList_clubs_condensed.qml

	plugins/Runs/qml/reports/competitorsWithCardRent.qml
	plugins/Runs/qml/reports/results_nstages.qml
	plugins/Runs/qml/reports/results_nstagesSpeaker.qml
	plugins/Runs/qml/reports/results_stage.qml
	plugins/Runs/qml/reports/results_stageSpeaker.qml
	plugins/Runs/qml/reports/startList_classes.qml
	plugins/Runs/qml/reports/startList_classes_nstages.qml
	plugins/Runs/qml/reports/startList_clubs.qml
	plugins/Runs/qml/reports/startList_clubs_nstages.qml
	plugins/Runs/qml/reports/startList_starters.qml
	plugins/Runs/qml/reports/awards/results_stage_awards-apple.qml
	plugins/Runs/qml/reports/awards/results_stage_awards-apple2.qml
	plugins/Runs/qml/reports/awards/results_stage_awards-covid.qml
	plugins/Runs/qml/reports/awards/results_stage_awards-hsh-2023.qml
	plugins/Runs/qml/reports/awards/results_stage_awards-hsh.qml
	plugins/Runs/qml/reports/awards/results_stage_awards.qml
	plugins/Runs/qml/reports/competitorsStatistics.qml

	plugins/shared/qml/reports/Cell.qml
	plugins/shared/qml/reports/HeaderCell.qml
	plugins/shared/qml/reports/QfObject.qml
	plugins/shared/qml/reports/QuickEventHeaderFooter.qml
	plugins/shared/qml/reports/QuickEventReportHeader.qml
	plugins/shared/qml/reports/ReportStyleCommon.qml
	plugins/shared/qml/reports/Space.qml
)

qt6_add_translation(QM_FILES
	quickevent-cs_CZ.ts
	quickevent-fr_FR.ts
	quickevent-nb_NO.ts
	quickevent-nl_BE.ts
	quickevent-pl_PL.ts
	quickevent-ru_RU.ts
	quickevent-es.ts
	quickevent-uk_UA.ts
)

qt6_add_lupdate(quickevent TS_FILES
	quickevent-cs_CZ.ts
	quickevent-fr_FR.ts
	quickevent-nb_NO.ts
	quickevent-nl_BE.ts
	quickevent-pl_PL.ts
	quickevent-ru_RU.ts
	quickevent-es.ts
	quickevent-uk_UA.ts
)

target_sources(quickevent PRIVATE ${QM_FILES})
target_include_directories(quickevent PRIVATE ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_link_libraries(quickevent PUBLIC libquickeventcore libquickeventgui libqfgui libsiut)

# Extract version from appversion.h
file(READ "${CMAKE_CURRENT_SOURCE_DIR}/src/appversion.h" APP_VERSION_H)
string(REGEX MATCH "#define APP_VERSION \"([0-9.]+)\"" _ ${APP_VERSION_H})
set(APP_VERSION ${CMAKE_MATCH_1})
message(STATUS "QuickEvent version: ${APP_VERSION}")

if(APPLE)
	target_link_libraries(quickevent PRIVATE iconv)

	# Create macOS icon from PNG
	set(ICON_SOURCE "${CMAKE_CURRENT_SOURCE_DIR}/images/quickevent64.png")
	set(ICON_OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/quickevent.icns")
	add_custom_command(
		OUTPUT ${ICON_OUTPUT}
		COMMAND mkdir -p quickevent.iconset
		COMMAND sips -z 16 16 ${ICON_SOURCE} --out quickevent.iconset/icon_16x16.png
		COMMAND sips -z 32 32 ${ICON_SOURCE} --out quickevent.iconset/icon_32x32.png
		COMMAND sips -z 128 128 ${ICON_SOURCE} --out quickevent.iconset/icon_128x128.png
		COMMAND sips -z 256 256 ${ICON_SOURCE} --out quickevent.iconset/icon_256x256.png
		COMMAND sips -z 512 512 ${ICON_SOURCE} --out quickevent.iconset/icon_512x512.png
		COMMAND iconutil -c icns quickevent.iconset -o ${ICON_OUTPUT}
		DEPENDS ${ICON_SOURCE}
		WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
		COMMENT "Generating macOS icon"
	)
	add_custom_target(quickevent_icon ALL DEPENDS ${ICON_OUTPUT})
	add_dependencies(quickevent quickevent_icon)

	set_target_properties(quickevent PROPERTIES
		MACOSX_BUNDLE TRUE
		MACOSX_BUNDLE_BUNDLE_NAME "QuickEvent"
		MACOSX_BUNDLE_GUI_IDENTIFIER "cz.quickevent.QuickEvent"
		MACOSX_BUNDLE_INFO_STRING "Quick Event - Orienteering Software"
		MACOSX_BUNDLE_SHORT_VERSION_STRING "${APP_VERSION}"
		MACOSX_BUNDLE_BUNDLE_VERSION "${APP_VERSION}"
		MACOSX_BUNDLE_ICON_FILE "quickevent.icns"
	)
	install(TARGETS quickevent BUNDLE DESTINATION .)
	install(FILES ${ICON_OUTPUT} DESTINATION quickevent.app/Contents/Resources)
	install(FILES ${QM_FILES} DESTINATION quickevent.app/Contents/MacOS/translations)

	foreach(plugin IN ITEMS Classes Runs Relays Receipts shared)
		install(DIRECTORY plugins/${plugin}/qml/reports
			DESTINATION quickevent.app/Contents/MacOS/reports/${plugin}/qml)
	endforeach()

	# Post-install: copy libraries, QML plugins, translations into bundle MacOS directory
	install(CODE "
		# Copy libraries to Frameworks
		file(GLOB LIBS \"${CMAKE_INSTALL_PREFIX}/lib/*.dylib\")
		file(COPY \${LIBS} DESTINATION \"${CMAKE_INSTALL_PREFIX}/quickevent.app/Contents/Frameworks\")

		# Copy QML and translations to MacOS (same structure as Linux)
		file(COPY \"${CMAKE_INSTALL_PREFIX}/bin/qml\" DESTINATION \"${CMAKE_INSTALL_PREFIX}/quickevent.app/Contents/MacOS\")
		file(COPY \"${CMAKE_INSTALL_PREFIX}/bin/translations\" DESTINATION \"${CMAKE_INSTALL_PREFIX}/quickevent.app/Contents/MacOS\")
	")
else()
	install(TARGETS quickevent)
	install(FILES ${QM_FILES} DESTINATION ${CMAKE_INSTALL_BINDIR}/translations)
	foreach(plugin IN ITEMS Classes Runs Relays Receipts shared)
		install(DIRECTORY plugins/${plugin}/qml/reports
			DESTINATION ${CMAKE_INSTALL_BINDIR}/reports/${plugin}/qml)
	endforeach()
endif()
