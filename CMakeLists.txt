cmake_minimum_required(VERSION 3.7.1)
project(cpp_streamer)

#set(CMAKE_CXX_FLAGS  "${CMAKE_CXX_FLAGS} -std=c++11 -g -Wno-deprecated -Wno-deprecated-declarations -Wno-reorder -Wall -fexceptions -frtti -D__STDC_FORMAT_MACROS -fPIC")
set(CMAKE_CXX_FLAGS  "${CMAKE_CXX_FLAGS} -std=c++14 -g -Wno-deprecated -Wno-deprecated-declarations -Wall -fexceptions -frtti -D__STDC_FORMAT_MACROS -fPIC")


set(CMAKE_OUTPUT_BASE ${CMAKE_BINARY_DIR}/output)
set(BUILD_OUTPUT_BASE ${CMAKE_BINARY_DIR}/output)
set(PREFIX_DIR "${BUILD_OUTPUT_BASE}")
set(INSTALL_RPATH "${PREFIX_DIR}/lib")

# set output static libary
SET(LIBRARY_OUTPUT_PATH ${CMAKE_BINARY_DIR}/output/lib)

# set pkgconfig path
set(CMAKE_PREFIX_PATH "${CMAKE_BINARY_DIR}/output/lib/pkgconfig")

if (CMAKE_SYSTEM_NAME STREQUAL "Darwin")
if (CMAKE_SYSTEM_PROCESSOR STREQUAL "arm64")
message("-----This is MacOs arm64")
else()
message("-----This is MacOs Intel")
endif()
else()
message("-----This is system ${CMAKE_SYSTEM_NAME}")
endif()
message("-----------------------------------------------")
message("-----------------------------------------------")
message("-----------------------------------------------")

# set include path
# Collect all subdirectories under src that contain header files and add them to include paths.
file(GLOB_RECURSE PROJECT_HEADER_FILES
    ${PROJECT_SOURCE_DIR}/src/*.h
    ${PROJECT_SOURCE_DIR}/src/*.hpp
)
set(SRC_INCLUDE_DIRS "")
foreach(_hdr ${PROJECT_HEADER_FILES})
    get_filename_component(_dir ${_hdr} DIRECTORY)
    list(APPEND SRC_INCLUDE_DIRS ${_dir})
endforeach()
list(REMOVE_DUPLICATES SRC_INCLUDE_DIRS)

include_directories(${CMAKE_BINARY_DIR}/output/include
                    ${CMAKE_BINARY_DIR}/output/include/openssl
                    ${PROJECT_SOURCE_DIR}/src
                    ${SRC_INCLUDE_DIRS}
                    /usr/local/include)

# set lib path
IF (APPLE)
link_directories(${CMAKE_BINARY_DIR}/output/lib /usr/local/lib)
ELSEIF (UNIX)
link_directories(${CMAKE_BINARY_DIR}/output/lib /usr/local/lib64 /usr/local/lib)
ENDIF ()

add_subdirectory(3rdparty)

################################################################
# RTC Pilot executable
add_executable(RTCPilot
            ${PROJECT_SOURCE_DIR}/src/format/amf/amf0.hpp
            ${PROJECT_SOURCE_DIR}/src/format/flv/flv_pub.hpp
            ${PROJECT_SOURCE_DIR}/src/format/flv/flv_pub.cpp
            ${PROJECT_SOURCE_DIR}/src/format/flv/flv_demux.hpp
            ${PROJECT_SOURCE_DIR}/src/format/flv/flv_demux.cpp
            ${PROJECT_SOURCE_DIR}/src/format/h264_h265_header.hpp
            ${PROJECT_SOURCE_DIR}/src/format/h264_h265_header.cpp
            ${PROJECT_SOURCE_DIR}/src/format/audio_header.hpp
            ${PROJECT_SOURCE_DIR}/src/format/audio_header.cpp
            ${PROJECT_SOURCE_DIR}/src/format/opus_header.hpp
            ${PROJECT_SOURCE_DIR}/src/format/opus_header.cpp
            ${PROJECT_SOURCE_DIR}/src/net/tcp/ssl_client.hpp
            ${PROJECT_SOURCE_DIR}/src/net/tcp/ssl_pub.hpp
            ${PROJECT_SOURCE_DIR}/src/net/tcp/ssl_server.hpp
            ${PROJECT_SOURCE_DIR}/src/net/tcp/tcp_client.hpp
            ${PROJECT_SOURCE_DIR}/src/net/tcp/tcp_pub.hpp
            ${PROJECT_SOURCE_DIR}/src/net/tcp/tcp_server.hpp
            ${PROJECT_SOURCE_DIR}/src/net/tcp/tcp_session.hpp
            ${PROJECT_SOURCE_DIR}/src/net/http/http_common.hpp
            ${PROJECT_SOURCE_DIR}/src/net/http/http_server.hpp
            ${PROJECT_SOURCE_DIR}/src/net/http/http_server.cpp
            ${PROJECT_SOURCE_DIR}/src/net/http/http_client.hpp
            ${PROJECT_SOURCE_DIR}/src/net/http/http_client.cpp
            ${PROJECT_SOURCE_DIR}/src/net/http/http_session.hpp
            ${PROJECT_SOURCE_DIR}/src/net/http/http_session.cpp
            ${PROJECT_SOURCE_DIR}/src/net/http/websocket/websocket_pub.hpp
            ${PROJECT_SOURCE_DIR}/src/net/http/websocket/websocket_pub.cpp
            ${PROJECT_SOURCE_DIR}/src/net/http/websocket/websocket_frame.hpp
            ${PROJECT_SOURCE_DIR}/src/net/http/websocket/websocket_frame.cpp
            ${PROJECT_SOURCE_DIR}/src/net/http/websocket/websocket_server.hpp
            ${PROJECT_SOURCE_DIR}/src/net/http/websocket/websocket_server.cpp
            ${PROJECT_SOURCE_DIR}/src/net/http/websocket/websocket_session.hpp
            ${PROJECT_SOURCE_DIR}/src/net/http/websocket/websocket_session.cpp
            ${PROJECT_SOURCE_DIR}/src/net/http/websocket/websocket_client.hpp
            ${PROJECT_SOURCE_DIR}/src/net/http/websocket/websocket_client.cpp
            ${PROJECT_SOURCE_DIR}/src/net/http/websocket/ws_session_base.hpp
            ${PROJECT_SOURCE_DIR}/src/net/http/websocket/ws_session_base.cpp
            ${PROJECT_SOURCE_DIR}/src/net/httpflv/httpflv_server.hpp
            ${PROJECT_SOURCE_DIR}/src/net/httpflv/httpflv_server.hpp
            ${PROJECT_SOURCE_DIR}/src/net/httpflv/httpflv_server.cpp
            ${PROJECT_SOURCE_DIR}/src/net/httpflv/httpflv_writer.hpp
            ${PROJECT_SOURCE_DIR}/src/net/httpflv/httpflv_writer.cpp
            ${PROJECT_SOURCE_DIR}/src/net/rtmp/chunk_stream.hpp
            ${PROJECT_SOURCE_DIR}/src/net/rtmp/chunk_stream.cpp
            ${PROJECT_SOURCE_DIR}/src/net/rtmp/rtmp_control_handler.hpp
            ${PROJECT_SOURCE_DIR}/src/net/rtmp/rtmp_control_handler.cpp
            ${PROJECT_SOURCE_DIR}/src/net/rtmp/rtmp_handshake.hpp
            ${PROJECT_SOURCE_DIR}/src/net/rtmp/rtmp_handshake.cpp
            ${PROJECT_SOURCE_DIR}/src/net/rtmp/rtmp_pub.hpp
            ${PROJECT_SOURCE_DIR}/src/net/rtmp/rtmp_request.hpp
            ${PROJECT_SOURCE_DIR}/src/net/rtmp/rtmp_server.hpp
            ${PROJECT_SOURCE_DIR}/src/net/rtmp/rtmp_server.cpp
            ${PROJECT_SOURCE_DIR}/src/net/rtmp/rtmp_session_base.hpp
            ${PROJECT_SOURCE_DIR}/src/net/rtmp/rtmp_session_base.cpp
            ${PROJECT_SOURCE_DIR}/src/net/rtmp/rtmp_session.hpp
            ${PROJECT_SOURCE_DIR}/src/net/rtmp/rtmp_session.cpp
            ${PROJECT_SOURCE_DIR}/src/net/rtmp/rtmp_writer.hpp
            ${PROJECT_SOURCE_DIR}/src/net/rtmp/rtmp_writer.cpp
            ${PROJECT_SOURCE_DIR}/src/net/rtprtcp/rtcp_fb_pub.hpp
            ${PROJECT_SOURCE_DIR}/src/net/rtprtcp/rtcp_pspli.hpp
            ${PROJECT_SOURCE_DIR}/src/net/rtprtcp/rtcp_rr.hpp
            ${PROJECT_SOURCE_DIR}/src/net/rtprtcp/rtcp_sr.hpp
            ${PROJECT_SOURCE_DIR}/src/net/rtprtcp/rtcp_xr_dlrr.hpp
            ${PROJECT_SOURCE_DIR}/src/net/rtprtcp/rtcp_xr_rrt.hpp
            ${PROJECT_SOURCE_DIR}/src/net/rtprtcp/rtcpfb_nack.hpp
            ${PROJECT_SOURCE_DIR}/src/net/rtprtcp/rtcp_tcc_fb.hpp
            ${PROJECT_SOURCE_DIR}/src/net/rtprtcp/rtp_packet.hpp
            ${PROJECT_SOURCE_DIR}/src/net/rtprtcp/rtp_packet.cpp
            ${PROJECT_SOURCE_DIR}/src/net/rtprtcp/rtprtcp_pub.hpp
            ${PROJECT_SOURCE_DIR}/src/format/rtc_sdp/rtc_sdp.hpp
            ${PROJECT_SOURCE_DIR}/src/format/rtc_sdp/rtc_sdp.cpp
            ${PROJECT_SOURCE_DIR}/src/format/rtc_sdp/rtc_sdp_filter.hpp
            ${PROJECT_SOURCE_DIR}/src/format/rtc_sdp/rtc_sdp_filter.cpp
            ${PROJECT_SOURCE_DIR}/src/format/rtc_sdp/rtc_sdp_media_section.hpp
            ${PROJECT_SOURCE_DIR}/src/format/rtc_sdp/rtc_sdp_pub.hpp
            ${PROJECT_SOURCE_DIR}/src/net/stun/stun.hpp
            ${PROJECT_SOURCE_DIR}/src/net/stun/stun.cpp
            ${PROJECT_SOURCE_DIR}/src/net/udp/udp_client.hpp
            ${PROJECT_SOURCE_DIR}/src/net/udp/udp_pub.hpp
            ${PROJECT_SOURCE_DIR}/src/net/udp/udp_server.hpp
            
            ${PROJECT_SOURCE_DIR}/src/webrtc_room/dtls_session.hpp
            ${PROJECT_SOURCE_DIR}/src/webrtc_room/dtls_session.cpp
            ${PROJECT_SOURCE_DIR}/src/webrtc_room/ice_server.hpp
            ${PROJECT_SOURCE_DIR}/src/webrtc_room/ice_server.cpp
            ${PROJECT_SOURCE_DIR}/src/webrtc_room/media_puller.hpp
            ${PROJECT_SOURCE_DIR}/src/webrtc_room/media_puller.cpp
            ${PROJECT_SOURCE_DIR}/src/webrtc_room/media_pusher.hpp
            ${PROJECT_SOURCE_DIR}/src/webrtc_room/media_pusher.cpp
            ${PROJECT_SOURCE_DIR}/src/webrtc_room/nack_generator.hpp
            ${PROJECT_SOURCE_DIR}/src/webrtc_room/nack_generator.cpp
            ${PROJECT_SOURCE_DIR}/src/webrtc_room/room.hpp
            ${PROJECT_SOURCE_DIR}/src/webrtc_room/room.cpp
            ${PROJECT_SOURCE_DIR}/src/webrtc_room/room_mgr.hpp
            ${PROJECT_SOURCE_DIR}/src/webrtc_room/room_mgr.cpp
            ${PROJECT_SOURCE_DIR}/src/webrtc_room/rtc_info.hpp
            ${PROJECT_SOURCE_DIR}/src/webrtc_room/rtc_user.hpp
            ${PROJECT_SOURCE_DIR}/src/webrtc_room/rtc_user.cpp
            ${PROJECT_SOURCE_DIR}/src/webrtc_room/rtp_recv_session.hpp
            ${PROJECT_SOURCE_DIR}/src/webrtc_room/rtp_recv_session.cpp
            ${PROJECT_SOURCE_DIR}/src/webrtc_room/rtp_send_session.hpp
            ${PROJECT_SOURCE_DIR}/src/webrtc_room/rtp_send_session.cpp
            ${PROJECT_SOURCE_DIR}/src/webrtc_room/rtp_session.hpp
            ${PROJECT_SOURCE_DIR}/src/webrtc_room/rtp_session.cpp
            ${PROJECT_SOURCE_DIR}/src/webrtc_room/srtp_session.hpp
            ${PROJECT_SOURCE_DIR}/src/webrtc_room/srtp_session.cpp
            ${PROJECT_SOURCE_DIR}/src/webrtc_room/udp_transport.hpp
            ${PROJECT_SOURCE_DIR}/src/webrtc_room/tcc_server.hpp
            ${PROJECT_SOURCE_DIR}/src/webrtc_room/tcc_server.cpp
            ${PROJECT_SOURCE_DIR}/src/webrtc_room/webrtc_server.hpp
            ${PROJECT_SOURCE_DIR}/src/webrtc_room/webrtc_server.cpp
            ${PROJECT_SOURCE_DIR}/src/webrtc_room/webrtc_session.hpp
            ${PROJECT_SOURCE_DIR}/src/webrtc_room/webrtc_session.cpp
            ${PROJECT_SOURCE_DIR}/src/webrtc_room/pilot_message_client.hpp
            ${PROJECT_SOURCE_DIR}/src/webrtc_room/pilot_message_client.cpp
            ${PROJECT_SOURCE_DIR}/src/webrtc_room/port_generator.hpp
            ${PROJECT_SOURCE_DIR}/src/webrtc_room/port_generator.cpp
            ${PROJECT_SOURCE_DIR}/src/webrtc_room/rtc_recv_relay.hpp
            ${PROJECT_SOURCE_DIR}/src/webrtc_room/rtc_recv_relay.cpp
            ${PROJECT_SOURCE_DIR}/src/webrtc_room/rtc_send_relay.hpp
            ${PROJECT_SOURCE_DIR}/src/webrtc_room/rtc_send_relay.cpp

            ${PROJECT_SOURCE_DIR}/src/ws_message/ws_message_server.hpp
            ${PROJECT_SOURCE_DIR}/src/ws_message/ws_message_server.cpp
            ${PROJECT_SOURCE_DIR}/src/ws_message/ws_message_session.hpp
            ${PROJECT_SOURCE_DIR}/src/ws_message/ws_message_session.cpp
            ${PROJECT_SOURCE_DIR}/src/ws_message/ws_protoo_info.hpp
            ${PROJECT_SOURCE_DIR}/src/ws_message/ws_protoo_client.hpp
            ${PROJECT_SOURCE_DIR}/src/ws_message/ws_protoo_client.cpp

            ${PROJECT_SOURCE_DIR}/src/utils/av/av.hpp
            ${PROJECT_SOURCE_DIR}/src/ws_stream/ws_play_session.hpp
            ${PROJECT_SOURCE_DIR}/src/ws_stream/ws_play_session.cpp
            ${PROJECT_SOURCE_DIR}/src/ws_stream/ws_publish_session.hpp
            ${PROJECT_SOURCE_DIR}/src/ws_stream/ws_publish_session.cpp
            ${PROJECT_SOURCE_DIR}/src/ws_stream/ws_stream_server.hpp
            ${PROJECT_SOURCE_DIR}/src/ws_stream/ws_stream_server.cpp
            ${PROJECT_SOURCE_DIR}/src/utils/av/gop_cache.hpp
            ${PROJECT_SOURCE_DIR}/src/utils/av/gop_cache.cpp
            ${PROJECT_SOURCE_DIR}/src/utils/av/media_packet.hpp
            ${PROJECT_SOURCE_DIR}/src/utils/av/media_statics.hpp
            ${PROJECT_SOURCE_DIR}/src/utils/av/media_stream_manager.hpp
            ${PROJECT_SOURCE_DIR}/src/utils/av/media_stream_manager.cpp
            ${PROJECT_SOURCE_DIR}/src/utils/base64.hpp
            ${PROJECT_SOURCE_DIR}/src/utils/base64.cpp
            ${PROJECT_SOURCE_DIR}/src/utils/byte_crypto.hpp
            ${PROJECT_SOURCE_DIR}/src/utils/byte_crypto.cpp
            ${PROJECT_SOURCE_DIR}/src/utils/byte_stream.hpp
            ${PROJECT_SOURCE_DIR}/src/utils/crc.hpp
            ${PROJECT_SOURCE_DIR}/src/utils/crc.cpp
            ${PROJECT_SOURCE_DIR}/src/utils/data_buffer.hpp
            ${PROJECT_SOURCE_DIR}/src/utils/io_interface.hpp
            ${PROJECT_SOURCE_DIR}/src/utils/ipaddress.hpp
            ${PROJECT_SOURCE_DIR}/src/utils/json.hpp
            ${PROJECT_SOURCE_DIR}/src/utils/logger.hpp
            ${PROJECT_SOURCE_DIR}/src/utils/stream_statics.hpp
            ${PROJECT_SOURCE_DIR}/src/utils/stringex.hpp
            ${PROJECT_SOURCE_DIR}/src/utils/timeex.hpp
            ${PROJECT_SOURCE_DIR}/src/utils/timeex.cpp
            ${PROJECT_SOURCE_DIR}/src/utils/timer.hpp
            ${PROJECT_SOURCE_DIR}/src/utils/timer.cpp
            ${PROJECT_SOURCE_DIR}/src/utils/uuid.hpp
            ${PROJECT_SOURCE_DIR}/src/config/config.hpp
            ${PROJECT_SOURCE_DIR}/src/config/config.cpp
            ${PROJECT_SOURCE_DIR}/src/cpp_streamer_interface.hpp
            ${PROJECT_SOURCE_DIR}/src/RTCPilot.cpp

            )
add_dependencies(RTCPilot openssl uv srtp2-ext yaml-cpp)
IF (APPLE)
target_link_libraries(RTCPilot dl z m ssl crypto srtp2 uv yaml-cpp)
ELSEIF (UNIX)
target_link_libraries(RTCPilot rt dl z m pthread ssl crypto srtp2 uv yaml-cpp)
ENDIF ()

# On macOS, set RPATH so the built executable can find dylibs in build/output/lib at runtime.
# We use @executable_path/output/lib because the executable is at build/ and the libs are at build/output/lib.
if(APPLE)
    # Enable usage of RPATH on macOS
    set(CMAKE_MACOSX_RPATH TRUE)

    # Ensure build and install RPATHs include the runtime relative path to output/lib
    set_target_properties(RTCPilot PROPERTIES
        BUILD_RPATH "@executable_path/output/lib"
        INSTALL_RPATH "@executable_path/output/lib"
        BUILD_WITH_INSTALL_RPATH TRUE
    )
endif()

################################################################
# tests: rtcp tcc fb
add_executable(rtcp_tcc_fb_test
    ${PROJECT_SOURCE_DIR}/tests/rtcp_tcc_fb_test.cpp
)
add_executable(timer_test
    ${PROJECT_SOURCE_DIR}/tests/timer_test.cpp
    ${PROJECT_SOURCE_DIR}/src/utils/timer.cpp
    ${PROJECT_SOURCE_DIR}/src/utils/timeex.cpp
)
add_dependencies(timer_test srtp2-ext uv)
IF (APPLE)
target_link_libraries(timer_test dl z m ssl crypto srtp2 uv yaml-cpp)
ELSEIF (UNIX)
target_link_libraries(timer_test rt dl z m pthread ssl crypto srtp2 uv yaml-cpp)
ENDIF ()

# Ensure tests inherit include directories
target_include_directories(rtcp_tcc_fb_test PRIVATE
    ${PROJECT_SOURCE_DIR}/src
    ${SRC_INCLUDE_DIRS}
)

if(APPLE)
    set_target_properties(rtcp_tcc_fb_test PROPERTIES
        BUILD_RPATH "@executable_path/output/lib"
        INSTALL_RPATH "@executable_path/output/lib"
        BUILD_WITH_INSTALL_RPATH TRUE
    )
endif()

################################################################
# Minimal test target: ws protoo client
# Keep this target light-weight: only the test source is compiled
add_executable(ws_protoo_client_test
    ${PROJECT_SOURCE_DIR}/tests/ws_protoo_client_test.cpp
    ${PROJECT_SOURCE_DIR}/src/ws_message/ws_protoo_client.cpp
    ${PROJECT_SOURCE_DIR}/src/ws_message/ws_message_session.cpp
    ${PROJECT_SOURCE_DIR}/src/net/http/websocket/websocket_client.cpp
    ${PROJECT_SOURCE_DIR}/src/net/http/websocket/websocket_frame.cpp
    ${PROJECT_SOURCE_DIR}/src/net/http/websocket/ws_session_base.cpp
    ${PROJECT_SOURCE_DIR}/src/net/http/websocket/websocket_pub.cpp
    ${PROJECT_SOURCE_DIR}/src/net/http/websocket/websocket_session.cpp
    ${PROJECT_SOURCE_DIR}/src/net/http/websocket/websocket_server.cpp
    ${PROJECT_SOURCE_DIR}/src/net/http/http_client.cpp
    ${PROJECT_SOURCE_DIR}/src/net/http/http_session.cpp
    ${PROJECT_SOURCE_DIR}/src/utils/timer.cpp
    ${PROJECT_SOURCE_DIR}/src/utils/timeex.cpp
    ${PROJECT_SOURCE_DIR}/src/utils/byte_crypto.cpp
    ${PROJECT_SOURCE_DIR}/src/utils/base64.cpp
)
add_dependencies(ws_protoo_client_test uv)

# Include project headers
target_include_directories(ws_protoo_client_test PRIVATE
    ${PROJECT_SOURCE_DIR}/src
    ${SRC_INCLUDE_DIRS}
)

# Link against system libs used by websocket/timer pieces
IF (APPLE)
    target_link_libraries(ws_protoo_client_test dl z m ssl crypto uv yaml-cpp)
ELSEIF (UNIX)
    target_link_libraries(ws_protoo_client_test rt dl z m pthread ssl crypto uv yaml-cpp)
ELSE()
    # On Windows, expect the environment/project to provide appropriate libs
ENDIF()

