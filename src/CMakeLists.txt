add_executable(ppcam ppcam.c)
target_include_directories(ppcam PRIVATE ${GSTREAMER_INCLUDE_DIRS})
target_link_libraries(ppcam ${GSTREAMER_LIBRARIES})
