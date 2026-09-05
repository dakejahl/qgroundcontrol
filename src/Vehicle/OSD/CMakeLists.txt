target_sources(${CMAKE_PROJECT_NAME} PRIVATE OsdController.cc OsdController.h OsdLayout.cc OsdLayout.h)

target_include_directories(${CMAKE_PROJECT_NAME} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

qt_add_resources(
    ${CMAKE_PROJECT_NAME} osd_schemas
    PREFIX "/json/osd"
    FILES osd.schema.json osd_layout.schema.json
)
