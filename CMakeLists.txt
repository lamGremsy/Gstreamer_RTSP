cmake_minimum_required(VERSION 3.8)
project(Gstreamer_RTSP)
find_package(PkgConfig REQUIRED)

pkg_check_modules(GSTLIBS REQUIRED
        gobject-2.0
        glib-2.0
        gstreamer-webrtc-1.0
        gstreamer-sdp-1.0
        gstreamer-pbutils-1.0
        libsoup-2.4
        json-glib-1.0
        gstreamer-rtsp-server-1.0
        gstreamer-check-1.0)

set(CMAKE_CXX_STANDARD 11)

pkg_check_modules(GLIB REQUIRED glib-2.0)
pkg_check_modules(GSTREAMER REQUIRED gstreamer-1.0)
pkg_check_modules(GSTREAMER_PLUGINS_BASE REQUIRED gstreamer-base-1.0)
pkg_check_modules(GSTREAMER_RTSP REQUIRED gstreamer-rtsp-1.0)
# pkg_check_modules(GTK REQUIRED gtk+-3.0)

include_directories (${GTK_INCLUDE_DIRS})
include_directories(${GLIB_INCLUDE_DIRS})
include_directories(${GSTREAMER_INCLUDE_DIRS} ${GSTREAMER_BASE_INCLUDE_DIRS})
include_directories(${GSTREAMER_RTSP_INCLUDE_DIRS})

set(GLIB_DEPS
    pcre
    gio-2.0
    glib-2.0
    gmodule-2.0
    gobject-2.0
    z)

set(GST_DEPS
    gstreamer-1.0
    gstbase-1.0
    gstvideo-1.0
    gstnet-1.0
    gstrtsp-1.0
    gstrtp-1.0
    gstsdp-1.0
    gstapp-1.0)

link_directories(${GSTLIBS_LIBRARY_DIRS})



add_executable(rtsp_stream_basic SourceCode/Stream/rtsp_stream_basic.cpp)
add_executable(rtsp_twocamera SourceCode/Stream/rtsp_twocamera.cpp)
add_executable(rtsp_twostream SourceCode/Stream/rtsp_twostream.cpp)
add_executable(rtsp_videotestsrc SourceCode/Stream/rtsp_videotestsrc.cpp)

add_executable(rtsp_twostream_record SourceCode/Stream_record/rtsp_twostream_record.cpp)

add_executable(rtsp_twostream_record_capture SourceCode/Stream_record_capture/rtsp_twostream_record_capture.cpp)
add_executable(rtsp_twocamera_multipleMode SourceCode/Stream_record_capture/rtsp_twocamera_multipleMode.cpp)




target_link_libraries(rtsp_stream_basic ${GST_DEPS} ${GLIB_DEPS} ${GSTLIBS_LIBRARIES})
target_link_libraries(rtsp_twocamera ${GST_DEPS} ${GLIB_DEPS} ${GSTLIBS_LIBRARIES})
target_link_libraries(rtsp_twostream ${GST_DEPS} ${GLIB_DEPS} ${GSTLIBS_LIBRARIES})
target_link_libraries(rtsp_videotestsrc ${GST_DEPS} ${GLIB_DEPS} ${GSTLIBS_LIBRARIES})

target_link_libraries(rtsp_twostream_record ${GST_DEPS} ${GLIB_DEPS} ${GSTLIBS_LIBRARIES})

target_link_libraries(rtsp_twostream_record_capture ${GST_DEPS} ${GLIB_DEPS} ${GSTLIBS_LIBRARIES})
target_link_libraries(rtsp_twocamera_multipleMode ${GST_DEPS} ${GLIB_DEPS} ${GSTLIBS_LIBRARIES})

