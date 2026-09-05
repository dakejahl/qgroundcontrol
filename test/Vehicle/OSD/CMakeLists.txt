target_sources(${CMAKE_PROJECT_NAME} PRIVATE OsdLayoutTest.cc OsdLayoutTest.h OsdControllerTest.cc OsdControllerTest.h)

target_include_directories(${CMAKE_PROJECT_NAME} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

add_qgc_test(OsdLayoutTest LABELS Unit Vehicle)
add_qgc_test(OsdControllerTest LABELS Integration Vehicle)
