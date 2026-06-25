add_library(libquickeventcore SHARED
    src/codedef.cpp
    src/coursedef.cpp
    src/exporters/fileexporter.cpp
    src/exporters/htmlfileexporter.cpp
    src/exporters/stageresultscsvexporter.cpp
    src/exporters/stageresultshtmlexporter.cpp
    src/exporters/stagestartlisthtmlexporter.cpp
    src/og/timems.cpp
    src/runstatus.cpp
    src/si/checkedcard.cpp
    src/si/checkedpunch.cpp
    src/si/punchrecord.cpp
    src/si/readcard.cpp
    src/si/siid.cpp
    src/utils.cpp
	libquickeventcore.qrc
    )

# Single source of truth for this project's translations: every
# libquickeventcore-<locale>.ts file in this directory. CONFIGURE_DEPENDS re-runs the
# glob at build time, so adding a new language is just dropping in a .ts file - no
# CMake edit, no silently skipped language. OUTPUT_LOCATION emits the compiled .qm
# next to the quickevent executable (see QE_TRANSLATIONS_OUTPUT_DIR).
file(GLOB TS_FILES CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/libquickeventcore-*.ts")
set_source_files_properties(${TS_FILES} PROPERTIES OUTPUT_LOCATION "${QE_TRANSLATIONS_OUTPUT_DIR}")
qt6_add_translation(QM_FILES ${TS_FILES})
qt6_add_lupdate(libquickeventcore TS_FILES ${TS_FILES})

target_sources(libquickeventcore PRIVATE ${QM_FILES})
target_compile_definitions(libquickeventcore PRIVATE QUICKEVENTCORE_BUILD_DLL)
target_link_libraries(libquickeventcore PUBLIC Qt::Core Qt::Sql libsiut)
target_include_directories(libquickeventcore PUBLIC include)

install(TARGETS libquickeventcore)
install(FILES ${QM_FILES} DESTINATION ${CMAKE_INSTALL_BINDIR}/translations)
