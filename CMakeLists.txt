cmake_minimum_required(VERSION 3.5.1)

set(SOURCE_FILES stream_decoder.h stream_decoder.c)
add_library(stream-decoder STATIC ${SOURCE_FILES})
set_property(TARGET stream-decoder PROPERTY C_STANDARD 99)

#add_executable(test_decode test.c ${SOURCE_FILES})
#target_link_libraries(test_decode avutil swscale swresample avformat avcodec va va-x11 va-drm dl rt m z pthread)
#set_property(TARGET test_decode PROPERTY C_STANDARD 99)
