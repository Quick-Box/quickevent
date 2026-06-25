add_library(libsiut SHARED
    src/device/crc529.c
    src/device/commport.cpp
    src/device/sidevicedriver.cpp
    src/device/sitask.cpp
    src/message/simessagedata.cpp
    src/sicard.cpp
    src/sipunch.cpp
    )

# Single source of truth for this project's translations: every libsiut-<locale>.ts
# file in this directory. CONFIGURE_DEPENDS re-runs the glob at build time, so adding
# a new language is just dropping in a .ts file - no CMake edit, no silently skipped
# language. OUTPUT_LOCATION emits the compiled .qm next to the quickevent executable
# (see QE_TRANSLATIONS_OUTPUT_DIR).
file(GLOB TS_FILES CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/libsiut-*.ts")
set_source_files_properties(${TS_FILES} PROPERTIES OUTPUT_LOCATION "${QE_TRANSLATIONS_OUTPUT_DIR}")
qt6_add_translation(QM_FILES ${TS_FILES})
qt6_add_lupdate(libsiut TS_FILES ${TS_FILES})

target_sources(libsiut PRIVATE ${QM_FILES})
target_compile_definitions(libsiut PRIVATE SIUT_BUILD_DLL)
target_link_libraries(libsiut PUBLIC Qt::Core Qt::SerialPort libqfcore)
target_include_directories(libsiut PUBLIC include)

install(TARGETS libsiut)
install(FILES ${QM_FILES} DESTINATION ${CMAKE_INSTALL_BINDIR}/translations)
