qt_add_qml_module(qfqmlreportsplugin
    URI "qf.qmlreports"
    VERSION "1.0"
	PLUGIN_TARGET qfqmlreportsplugin
	RESOURCE_PREFIX qml/qf/qmlreports
#    PAST_MAJOR_VERSIONS 1
#    CLASS_NAME QtWebSocketsDeclarativeModule
#    NO_PLUGIN_OPTIONAL
    NO_GENERATE_PLUGIN_SOURCE
#    NO_GENERATE_QMLTYPES
#    INSTALL_SOURCE_QMLTYPES "plugins.qmltypes"
)


target_sources(qfqmlreportsplugin PRIVATE
	src/qfqmlreports_plugin.cpp
	src/qfqmlreports_plugin.h
	src/reportitemextensions.cpp
	src/reportitemextensions.h
)

target_link_libraries(qfqmlreportsplugin PUBLIC
    Qt::Core
    Qt::Gui
    Qt::Qml
	libqfgui
)

set(PLUGIN_DEST_DIR ${CMAKE_INSTALL_BINDIR}/qml/qf/qmlreports)
install(TARGETS qfqmlreportsplugin LIBRARY DESTINATION ${PLUGIN_DEST_DIR})
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/qmldir DESTINATION ${PLUGIN_DEST_DIR})

