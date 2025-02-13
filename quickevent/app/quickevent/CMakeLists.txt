add_executable(quickevent
	plugins/CardReader/src/cardchecker.cpp
	plugins/CardReader/src/cardcheckerclassiccpp.cpp
	plugins/CardReader/src/cardcheckerfreeordercpp.cpp
	plugins/CardReader/src/cardreaderplugin.cpp
	plugins/CardReader/src/cardreaderwidget.cpp
	plugins/CardReader/src/services/mqttpunches.cpp
	plugins/CardReader/src/services/mqttpuncheswidget.cpp
	plugins/CardReader/src/services/racomclient.cpp
	plugins/CardReader/src/services/racomclientwidget.cpp
	plugins/CardReader/src/cardreadersettingspage.cpp
	plugins/CardReader/src/cardreadersettings.cpp

	plugins/CardReader/src/services/qropunch.cpp
	plugins/CardReader/src/services/qropunchwidget.cpp
	plugins/CardReader/src/services/qropunchwidget.ui

	plugins/Classes/src/classdefdocument.cpp
	plugins/Classes/src/classdefwidget.cpp
	plugins/Classes/src/classdocument.cpp
	plugins/Classes/src/classesplugin.cpp
	plugins/Classes/src/classestableview.cpp
	plugins/Classes/src/classeswidget.cpp
	plugins/Classes/src/drawing/classitem.cpp
	plugins/Classes/src/drawing/drawingganttwidget.cpp
	plugins/Classes/src/drawing/ganttitem.cpp
	plugins/Classes/src/drawing/ganttruler.cpp
	plugins/Classes/src/drawing/ganttscene.cpp
	plugins/Classes/src/drawing/ganttview.cpp
	plugins/Classes/src/drawing/iganttitem.cpp
	plugins/Classes/src/drawing/startslotheader.cpp
	plugins/Classes/src/drawing/startslotitem.cpp
	plugins/Classes/src/editcodeswidget.cpp
	plugins/Classes/src/editcoursecodeswidget.cpp
	plugins/Classes/src/editcourseswidget.cpp
	plugins/Classes/src/importcoursedef.cpp
	plugins/Competitors/src/competitordocument.cpp
	plugins/Competitors/src/competitorsplugin.cpp
	plugins/Competitors/src/competitorswidget.cpp
	plugins/Competitors/src/competitorwidget.cpp
	plugins/Competitors/src/findregistrationedit.cpp
	plugins/Competitors/src/lentcardssettingspage.cpp
	plugins/Competitors/src/registrationswidget.cpp
	plugins/Competitors/src/stationsbackupmemorywidget.cpp
	plugins/Core/src/coreplugin.cpp
	plugins/Core/src/settings.cpp
	plugins/Core/src/widgets/reportssettingspage.cpp
	plugins/Core/src/widgets/appstatusbar.cpp
	plugins/Core/src/widgets/settingspage.cpp
	plugins/Core/src/widgets/settingsdialog.cpp
	plugins/Event/src/connectdbdialogwidget.cpp
	plugins/Event/src/connectionsettings.cpp
	plugins/Event/src/dbschema.cpp
	plugins/Event/src/eventconfig.cpp
	plugins/Event/src/eventdialogwidget.cpp
	plugins/Event/src/eventplugin.cpp

	plugins/Event/src/services/emmaclient.cpp
	plugins/Event/src/services/emmaclientwidget.cpp
	plugins/Event/src/services/oresultsclient.cpp
	plugins/Event/src/services/oresultsclientwidget.cpp
	plugins/Event/src/services/ofeed/ofeedclient.cpp
	plugins/Event/src/services/ofeed/ofeedclientwidget.cpp
	plugins/Event/src/services/service.cpp
	plugins/Event/src/services/serviceswidget.cpp
	plugins/Event/src/services/servicewidget.cpp
	plugins/Event/src/services/qx/qxclientservice.cpp plugins/Event/src/services/qx/qxclientservice.h
	plugins/Event/src/services/qx/qxclientservicewidget.cpp plugins/Event/src/services/qx/qxclientservicewidget.h plugins/Event/src/services/qx/qxclientservicewidget.ui


	plugins/Event/src/stage.cpp
	plugins/Event/src/stagedocument.cpp
	plugins/Event/src/stagewidget.cpp
	plugins/Oris/src/chooseoriseventdialog.cpp
	plugins/Oris/src/orisimporter.cpp
	plugins/Oris/src/orisplugin.cpp
	plugins/Oris/src/txtimporter.cpp
	plugins/Oris/src/xmlimporter.cpp
	plugins/Receipts/src/receiptsplugin.cpp
	plugins/Receipts/src/receiptsprinter.cpp
	#plugins/Receipts/src/receiptsprinteroptions.cpp
	plugins/Receipts/src/receiptsprinteroptionsdialog.cpp
	plugins/Receipts/src/receiptswidget.cpp
	plugins/Receipts/src/receiptssettings.cpp
	plugins/Receipts/src/receiptssettingspage.cpp
	plugins/Relays/src/addlegdialogwidget.cpp
	plugins/Relays/src/relaydocument.cpp
	plugins/Relays/src/relaysplugin.cpp
	plugins/Relays/src/relayswidget.cpp
	plugins/Relays/src/relaywidget.cpp
	plugins/Runs/src/cardflagsdialog.cpp
	plugins/Runs/src/eventstatisticsoptions.cpp
	plugins/Runs/src/eventstatisticswidget.cpp
	plugins/Runs/src/findrunneredit.cpp
	plugins/Runs/src/findrunnerwidget.cpp
	plugins/Runs/src/nstagesreportoptionsdialog.cpp
	plugins/Runs/src/printawardsoptionsdialogwidget.cpp
	plugins/Runs/src/runflagsdialog.cpp
	plugins/Runs/src/runsplugin.cpp
	plugins/Runs/src/runstabledialogwidget.cpp
	plugins/Runs/src/runstableitemdelegate.cpp
	plugins/Runs/src/runstablemodel.cpp
	plugins/Runs/src/runstablewidget.cpp
	plugins/Runs/src/runswidget.cpp
	plugins/Runs/src/services/resultsexporter.cpp
	plugins/Runs/src/services/resultsexporterwidget.cpp
	plugins/Speaker/src/codeclassresultswidget.cpp
	plugins/Speaker/src/punchestableview.cpp
	plugins/Speaker/src/speakerplugin.cpp
	plugins/Speaker/src/speakerwidget.cpp

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
)

qt6_add_translation(QM_FILES
	quickevent-cs_CZ.ts
	quickevent-fr_FR.ts
	quickevent-nb_NO.ts
	quickevent-nl_BE.ts
	quickevent-pl_PL.ts
	quickevent-ru_RU.ts
	quickevent-uk_UA.ts
)

qt_add_lupdate(quickevent TS_FILES
	quickevent-cs_CZ.ts
	quickevent-fr_FR.ts
	quickevent-nb_NO.ts
	quickevent-nl_BE.ts
	quickevent-pl_PL.ts
	quickevent-ru_RU.ts
	quickevent-uk_UA.ts
)

target_sources(quickevent PRIVATE ${QM_FILES})
target_include_directories(quickevent PRIVATE ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_link_libraries(quickevent PUBLIC libquickeventcore libquickeventgui libqfqmlwidgets libsiut)

install(TARGETS quickevent)
install(FILES ${QM_FILES} DESTINATION ${CMAKE_INSTALL_BINDIR}/translations)

#install(DIRECTORY plugins/Runs/qml/reports DESTINATION ${CMAKE_INSTALL_BINDIR}/reports/Runs/qml)
#install(DIRECTORY plugins/Receipts/qml/reports DESTINATION ${CMAKE_INSTALL_BINDIR}/reports/Receipts/qml)
foreach(plugin IN ITEMS Classes Competitors Runs Relays Receipts shared)
	install(DIRECTORY plugins/${plugin}/qml/reports DESTINATION ${CMAKE_INSTALL_BINDIR}/reports/${plugin}/qml)
endforeach()
