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


# Single source of truth for this project's translations: every
# libquickeventgui-<locale>.ts file in this directory. CONFIGURE_DEPENDS re-runs the
# glob at build time, so adding a new language is just dropping in a .ts file - no
# CMake edit, no silently skipped language. OUTPUT_LOCATION emits the compiled .qm
# next to the quickevent executable (see QE_TRANSLATIONS_OUTPUT_DIR).
file(GLOB TS_FILES CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/libquickeventgui-*.ts")
set_source_files_properties(${TS_FILES} PROPERTIES OUTPUT_LOCATION "${QE_TRANSLATIONS_OUTPUT_DIR}")
qt6_add_translation(QM_FILES ${TS_FILES})
qt6_add_lupdate(libquickeventgui TS_FILES ${TS_FILES})

target_sources(libquickeventgui PRIVATE ${QM_FILES})
target_compile_definitions(libquickeventgui PRIVATE QUICKEVENTGUI_BUILD_DLL)
target_link_libraries(libquickeventgui PUBLIC Qt::Core Qt::Multimedia libquickeventcore libqfgui)
target_include_directories(libquickeventgui PUBLIC include)

install(TARGETS libquickeventgui)
install(FILES ${QM_FILES} DESTINATION ${CMAKE_INSTALL_BINDIR}/translations)
