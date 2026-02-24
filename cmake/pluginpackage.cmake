# Package the plugin into a distributable ZIP
set(CPACK_GENERATOR "ZIP")
set(CPACK_PACKAGE_NAME "${PROJECT_NAME}")
set(CPACK_PACKAGE_VERSION "${PROJECT_VERSION}")
set(CPACK_PACKAGE_FILE_NAME "${PROJECT_NAME}-${PROJECT_VERSION}")

install(
    FILES "$<TARGET_FILE:${PROJECT_NAME}>"
    DESTINATION "SKSE/Plugins"
)
install(
    FILES "${CMAKE_CURRENT_SOURCE_DIR}/dist/Data/SKSE/Plugins/SkyrimLoadAccelerator.ini"
    DESTINATION "SKSE/Plugins"
)

include(CPack)
