add_library(libqfgui SHARED
    src/action.cpp
    src/actiongroup.cpp
    src/checkbox.cpp
    src/combobox.cpp
    src/datacontroller.cpp
    src/dateedit.cpp
    src/datetimeedit.cpp
    src/dialogbuttonbox.cpp

    src/model/datadocument.cpp
    src/model/logtablemodel.cpp
    src/model/sqldatadocument.cpp
    src/model/sqltablemodel.cpp
    src/model/tablemodel.cpp

    src/dialogs/dialog.cpp
    src/dialogs/filedialog.cpp
    src/dialogs/getiteminputdialog.cpp
    src/dialogs/inputdialog.cpp
    src/dialogs/internal/captionframe.cpp
    src/dialogs/messagebox.cpp
    src/dialogs/previewdialog.cpp
    src/dialogs/previewdialog.ui
    src/dialogs/qmldialog.cpp
    src/exportcsvtableviewwidget.cpp
    src/exporttableviewwidget.cpp
    src/exporttableviewwidget.ui
    src/frame.cpp
    src/framework/application.cpp
    src/framework/centralwidget.cpp
    src/framework/cursoroverrider.cpp
    src/framework/datadialogwidget.cpp
    src/framework/dialogwidget.cpp
    src/framework/dockablewidget.cpp
    src/framework/dockwidget.cpp
    src/framework/ipersistentoptions.cpp
    src/framework/ipersistentsettings.cpp
    src/framework/logwidget.cpp
    src/framework/logwidget.ui
    src/framework/mainwindow.cpp
    src/framework/partswitch.cpp
    src/framework/partwidget.cpp
    src/framework/plugin.cpp
    src/framework/pluginmanifest.cpp
    src/framework/stackedcentralwidget.cpp
    src/graphics/graphics.cpp
    src/headerview.cpp
    src/htmlviewwidget.cpp
    src/htmlviewwidget.ui
    src/idatawidget.cpp
    src/internal/desktoputils.cpp
    src/internal/dlgtableviewcopyspecial.cpp
    src/internal/dlgtableviewcopyspecial.ui
    src/internal/tableviewchoosecolumnswidget.cpp
    src/internal/tableviewchoosecolumnswidget.ui
    src/internal/tableviewcopytodialogwidget.cpp
    src/internal/tableviewcopytodialogwidget.ui
    src/label.cpp
    src/layoutpropertiesattached.cpp
    src/layouttypeproperties.cpp
    src/lineedit.cpp
    src/menubar.cpp
    src/progressbar.cpp
    src/reports/processor/banddatamodel.cpp
    src/reports/processor/reportdocument.cpp
    src/reports/processor/reportitem.cpp
    src/reports/processor/reportitem_html.cpp
    src/reports/processor/reportitemband.cpp
    src/reports/processor/reportitembreak.cpp
    src/reports/processor/reportitemdetail.cpp
    src/reports/processor/reportitemframe.cpp
    src/reports/processor/reportitemimage.cpp
    src/reports/processor/reportitempara.cpp
    src/reports/processor/reportitemreport.cpp
    src/reports/processor/reportpainter.cpp
    src/reports/processor/reportprocessor.cpp
    src/reports/processor/style/brush.cpp
    src/reports/processor/style/color.cpp
    src/reports/processor/style/compiledtextstyle.cpp
    src/reports/processor/style/font.cpp
    src/reports/processor/style/pen.cpp
    src/reports/processor/style/sheet.cpp
    src/reports/processor/style/styleobject.cpp
    src/reports/processor/style/text.cpp
    src/reports/widgets/itemvalueeditorwidget.cpp
    src/reports/widgets/itemvalueeditorwidget.ui
    src/reports/widgets/printtableviewwidget/printtableviewwidget.cpp
    src/reports/widgets/printtableviewwidget/printtableviewwidget.ui
    src/reports/widgets/reportviewwidget.cpp
    src/saveoptionswidget.cpp
    src/saveoptionswidget.ui
    src/spinbox.cpp
    src/splitter.cpp
    src/sqltableitemdelegate.cpp
    src/statusbar.cpp
    src/style.cpp
    src/tableitemdelegate.cpp
    src/tableview.cpp
    src/tableviewproxymodel.cpp
    src/tableviewtoolbar.cpp
    src/textedit.cpp
    src/texteditwidget.cpp
    src/texteditwidget.ui
    src/timeedit.cpp
    src/toolbar.cpp
    style/qf_gui_style.qrc

    images/qf_gui_images.qrc
	images/flat/qf_gui_images_flat.qrc
	images/lucide/qf_gui_images_lucide.qrc
	src/reports/widgets/printtableviewwidget/reports/qf_gui_printtablewidget_reports.qrc
    )

qt6_add_translations(libqfgui
    TS_FILES
        libqfgui_cs_CZ.ts
        libqfgui_fr_FR.ts
        libqfgui_nb_NO.ts
        libqfgui_nl_BE.ts
        libqfgui_pl_PL.ts
        libqfgui_ru_RU.ts
        libqfgui_uk_UA.ts
	)

target_sources(libqfgui PRIVATE ${QM_FILES})

target_link_libraries(libqfgui PUBLIC Qt::Svg Qt::PrintSupport Qt::Sql Qt::Qml Qt::Widgets Qt::Xml libqfcore)
target_include_directories(libqfgui PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_include_directories(libqfgui PUBLIC include)
target_compile_definitions(libqfgui PRIVATE QFGUI_BUILD_DLL)

install(TARGETS libqfgui)
install(FILES ${QM_FILES} DESTINATION ${CMAKE_INSTALL_BINDIR}/translations)


