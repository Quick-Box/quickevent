add_library(libquickeventgui SHARED
    src/audio/player.cpp
    src/og/itemdelegate.cpp
    src/og/sqltablemodel.cpp
    src/og/timeedit.cpp
    src/reportoptionsdialog.cpp
    src/reportoptionsdialog.ui
    src/si/siidedit.cpp
    libquickeventgui.qrc
    )

qt6_add_translation(QM_FILES
	libquickeventgui_cs_CZ.ts
	libquickeventgui_fr_FR.ts
	libquickeventgui_nb_NO.ts
	libquickeventgui_nl_BE.ts
	libquickeventgui_pl_PL.ts
	libquickeventgui_ru_RU.ts
	libquickeventgui_uk_UA.ts
	)

qt6_add_lupdate(libquickeventgui TS_FILES
	libquickeventgui_cs_CZ.ts
	libquickeventgui_fr_FR.ts
	libquickeventgui_nb_NO.ts
	libquickeventgui_nl_BE.ts
	libquickeventgui_pl_PL.ts
	libquickeventgui_ru_RU.ts
	libquickeventgui_uk_UA.ts
	)

target_sources(libquickeventgui PRIVATE ${QM_FILES})
target_compile_definitions(libquickeventgui PRIVATE QUICKEVENTGUI_BUILD_DLL)
target_link_libraries(libquickeventgui PUBLIC Qt::Core Qt::Multimedia libquickeventcore libqfgui)
target_include_directories(libquickeventgui PUBLIC include)

install(TARGETS libquickeventgui)
install(FILES ${QM_FILES} DESTINATION ${CMAKE_INSTALL_BINDIR}/translations)
