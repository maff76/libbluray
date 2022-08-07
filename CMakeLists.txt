cmake_minimum_required(VERSION 3.0)

set(BLURAY_VERSION_MAJOR 1)
set(BLURAY_VERSION_MINOR 3)
set(BLURAY_VERSION_MICRO 2)

project(libbluray VERSION ${BLURAY_VERSION_MAJOR}.${BLURAY_VERSION_MINOR}.${BLURAY_VERSION_MICRO} LANGUAGES C)

configure_file(${CMAKE_CURRENT_SOURCE_DIR}/src/libbluray/bluray-version.h.in
               ${CMAKE_CURRENT_SOURCE_DIR}/src/libbluray/bluray-version.h)

find_package(freetype 2.6 REQUIRED NO_MODULE)
find_package(iconv 1 REQUIRED NO_MODULE)
find_package(libxml2 2.9 REQUIRED NO_MODULE)
find_package(libudfread 1.1.2 REQUIRED NO_MODULE)

if(MSVC)
  set(CMAKE_DEBUG_POSTFIX "d")
endif()

set(SRCS
  ${CMAKE_CURRENT_BINARY_DIR}/config.h
  ${CMAKE_CURRENT_SOURCE_DIR}/cmake/libbluray.def
  ${CMAKE_CURRENT_SOURCE_DIR}/src/file/dir_win32.c
  ${CMAKE_CURRENT_SOURCE_DIR}/src/file/dirs_win32.c
  ${CMAKE_CURRENT_SOURCE_DIR}/src/file/dirs.h
  ${CMAKE_CURRENT_SOURCE_DIR}/src/file/dl_win32.c
  ${CMAKE_CURRENT_SOURCE_DIR}/src/file/dl.h
  ${CMAKE_CURRENT_SOURCE_DIR}/src/file/file_win32.c
  ${CMAKE_CURRENT_SOURCE_DIR}/src/file/file.c
  ${CMAKE_CURRENT_SOURCE_DIR}/src/file/file.h
  ${CMAKE_CURRENT_SOURCE_DIR}/src/file/filesystem.c
  ${CMAKE_CURRENT_SOURCE_DIR}/src/file/filesystem.h
  ${CMAKE_CURRENT_SOURCE_DIR}/src/file/mount.c
  ${CMAKE_CURRENT_SOURCE_DIR}/src/file/mount.h
  ${CMAKE_CURRENT_SOURCE_DIR}/src/libbluray/bdj/bdj.c
  ${CMAKE_CURRENT_SOURCE_DIR}/src/libbluray/bdj/bdj.h
  ${CMAKE_CURRENT_SOURCE_DIR}/src/libbluray/bdj/bdjo_data.h
  ${CMAKE_CURRENT_SOURCE_DIR}/src/libbluray/bdj/bdjo_parse.c
  ${CMAKE_CURRENT_SOURCE_DIR}/src/libbluray/bdj/bdjo_parse.h
  ${CMAKE_CURRENT_SOURCE_DIR}/src/libbluray/bdj/native/bdjo.c
  ${CMAKE_CURRENT_SOURCE_DIR}/src/libbluray/bdj/native/bdjo.h
  ${CMAKE_CURRENT_SOURCE_DIR}/src/libbluray/bdj/native/java_awt_BDFontMetrics.c
  ${CMAKE_CURRENT_SOURCE_DIR}/src/libbluray/bdj/native/java_awt_BDFontMetrics.h
  ${CMAKE_CURRENT_SOURCE_DIR}/src/libbluray/bdj/native/java_awt_BDGraphics.c
  ${CMAKE_CURRENT_SOURCE_DIR}/src/libbluray/bdj/native/java_awt_BDGraphics.h
  ${CMAKE_CURRENT_SOURCE_DIR}/src/libbluray/bdj/native/org_videolan_Libbluray.c
  ${CMAKE_CURRENT_SOURCE_DIR}/src/libbluray/bdj/native/org_videolan_Libbluray.h
  ${CMAKE_CURRENT_SOURCE_DIR}/src/libbluray/bdj/native/org_videolan_Logger.c
  ${CMAKE_CURRENT_SOURCE_DIR}/src/libbluray/bdj/native/org_videolan_Logger.h
  ${CMAKE_CURRENT_SOURCE_DIR}/src/libbluray/bdj/native/register_native.c
  ${CMAKE_CURRENT_SOURCE_DIR}/src/libbluray/bdj/native/register_native.h
  ${CMAKE_CURRENT_SOURCE_DIR}/src/libbluray/bdj/native/util.c
  ${CMAKE_CURRENT_SOURCE_DIR}/src/libbluray/bdj/native/util.h
  ${CMAKE_CURRENT_SOURCE_DIR}/src/libbluray/bdnav/bdid_parse.c
  ${CMAKE_CURRENT_SOURCE_DIR}/src/libbluray/bdnav/bdid_parse.h
  ${CMAKE_CURRENT_SOURCE_DIR}/src/libbluray/bdnav/bdmv_parse.c
  ${CMAKE_CURRENT_SOURCE_DIR}/src/libbluray/bdnav/bdmv_parse.h
  ${CMAKE_CURRENT_SOURCE_DIR}/src/libbluray/bdnav/bdparse.h
  ${CMAKE_CURRENT_SOURCE_DIR}/src/libbluray/bdnav/clpi_data.h
  ${CMAKE_CURRENT_SOURCE_DIR}/src/libbluray/bdnav/clpi_parse.c
  ${CMAKE_CURRENT_SOURCE_DIR}/src/libbluray/bdnav/clpi_parse.h
  ${CMAKE_CURRENT_SOURCE_DIR}/src/libbluray/bdnav/extdata_parse.c
  ${CMAKE_CURRENT_SOURCE_DIR}/src/libbluray/bdnav/extdata_parse.h
  ${CMAKE_CURRENT_SOURCE_DIR}/src/libbluray/bdnav/index_parse.c
  ${CMAKE_CURRENT_SOURCE_DIR}/src/libbluray/bdnav/index_parse.h
  ${CMAKE_CURRENT_SOURCE_DIR}/src/libbluray/bdnav/meta_data.h
  ${CMAKE_CURRENT_SOURCE_DIR}/src/libbluray/bdnav/meta_parse.c
  ${CMAKE_CURRENT_SOURCE_DIR}/src/libbluray/bdnav/meta_parse.h
  ${CMAKE_CURRENT_SOURCE_DIR}/src/libbluray/bdnav/mpls_data.h
  ${CMAKE_CURRENT_SOURCE_DIR}/src/libbluray/bdnav/mpls_parse.c
  ${CMAKE_CURRENT_SOURCE_DIR}/src/libbluray/bdnav/mpls_parse.h
  ${CMAKE_CURRENT_SOURCE_DIR}/src/libbluray/bdnav/navigation.c
  ${CMAKE_CURRENT_SOURCE_DIR}/src/libbluray/bdnav/navigation.h
  ${CMAKE_CURRENT_SOURCE_DIR}/src/libbluray/bdnav/sound_parse.c
  ${CMAKE_CURRENT_SOURCE_DIR}/src/libbluray/bdnav/sound_parse.h
  ${CMAKE_CURRENT_SOURCE_DIR}/src/libbluray/bdnav/uo_mask_table.h
  ${CMAKE_CURRENT_SOURCE_DIR}/src/libbluray/bdnav/uo_mask.c
  ${CMAKE_CURRENT_SOURCE_DIR}/src/libbluray/bdnav/uo_mask.h
  ${CMAKE_CURRENT_SOURCE_DIR}/src/libbluray/bluray_internal.h
  ${CMAKE_CURRENT_SOURCE_DIR}/src/libbluray/bluray-version.h
  ${CMAKE_CURRENT_SOURCE_DIR}/src/libbluray/bluray.c
  ${CMAKE_CURRENT_SOURCE_DIR}/src/libbluray/bluray.h
  ${CMAKE_CURRENT_SOURCE_DIR}/src/libbluray/decoders/graphics_controller.c
  ${CMAKE_CURRENT_SOURCE_DIR}/src/libbluray/decoders/graphics_controller.h
  ${CMAKE_CURRENT_SOURCE_DIR}/src/libbluray/decoders/graphics_processor.c
  ${CMAKE_CURRENT_SOURCE_DIR}/src/libbluray/decoders/graphics_processor.h
  ${CMAKE_CURRENT_SOURCE_DIR}/src/libbluray/decoders/hdmv_pids.h
  ${CMAKE_CURRENT_SOURCE_DIR}/src/libbluray/decoders/ig_decode.c
  ${CMAKE_CURRENT_SOURCE_DIR}/src/libbluray/decoders/ig_decode.h
  ${CMAKE_CURRENT_SOURCE_DIR}/src/libbluray/decoders/ig.h
  ${CMAKE_CURRENT_SOURCE_DIR}/src/libbluray/decoders/m2ts_demux.c
  ${CMAKE_CURRENT_SOURCE_DIR}/src/libbluray/decoders/m2ts_demux.h
  ${CMAKE_CURRENT_SOURCE_DIR}/src/libbluray/decoders/m2ts_filter.c
  ${CMAKE_CURRENT_SOURCE_DIR}/src/libbluray/decoders/m2ts_filter.h
  ${CMAKE_CURRENT_SOURCE_DIR}/src/libbluray/decoders/overlay.h
  ${CMAKE_CURRENT_SOURCE_DIR}/src/libbluray/decoders/pes_buffer.c
  ${CMAKE_CURRENT_SOURCE_DIR}/src/libbluray/decoders/pes_buffer.h
  ${CMAKE_CURRENT_SOURCE_DIR}/src/libbluray/decoders/pg_decode.c
  ${CMAKE_CURRENT_SOURCE_DIR}/src/libbluray/decoders/pg_decode.h
  ${CMAKE_CURRENT_SOURCE_DIR}/src/libbluray/decoders/pg.h
  ${CMAKE_CURRENT_SOURCE_DIR}/src/libbluray/decoders/rle.c
  ${CMAKE_CURRENT_SOURCE_DIR}/src/libbluray/decoders/rle.h
  ${CMAKE_CURRENT_SOURCE_DIR}/src/libbluray/decoders/textst_decode.c
  ${CMAKE_CURRENT_SOURCE_DIR}/src/libbluray/decoders/textst_decode.h
  ${CMAKE_CURRENT_SOURCE_DIR}/src/libbluray/decoders/textst_render.c
  ${CMAKE_CURRENT_SOURCE_DIR}/src/libbluray/decoders/textst_render.h
  ${CMAKE_CURRENT_SOURCE_DIR}/src/libbluray/decoders/textst.h
  ${CMAKE_CURRENT_SOURCE_DIR}/src/libbluray/disc/aacs.c
  ${CMAKE_CURRENT_SOURCE_DIR}/src/libbluray/disc/aacs.h
  ${CMAKE_CURRENT_SOURCE_DIR}/src/libbluray/disc/bdplus.c
  ${CMAKE_CURRENT_SOURCE_DIR}/src/libbluray/disc/bdplus.h
  ${CMAKE_CURRENT_SOURCE_DIR}/src/libbluray/disc/dec.c
  ${CMAKE_CURRENT_SOURCE_DIR}/src/libbluray/disc/dec.h
  ${CMAKE_CURRENT_SOURCE_DIR}/src/libbluray/disc/disc.c
  ${CMAKE_CURRENT_SOURCE_DIR}/src/libbluray/disc/disc.h
  ${CMAKE_CURRENT_SOURCE_DIR}/src/libbluray/disc/enc_info.h
  ${CMAKE_CURRENT_SOURCE_DIR}/src/libbluray/disc/properties.c
  ${CMAKE_CURRENT_SOURCE_DIR}/src/libbluray/disc/properties.h
  ${CMAKE_CURRENT_SOURCE_DIR}/src/libbluray/disc/udf_fs.c
  ${CMAKE_CURRENT_SOURCE_DIR}/src/libbluray/disc/udf_fs.h
  ${CMAKE_CURRENT_SOURCE_DIR}/src/libbluray/hdmv/hdmv_insn.h
  ${CMAKE_CURRENT_SOURCE_DIR}/src/libbluray/hdmv/hdmv_vm.c
  ${CMAKE_CURRENT_SOURCE_DIR}/src/libbluray/hdmv/hdmv_vm.h
  ${CMAKE_CURRENT_SOURCE_DIR}/src/libbluray/hdmv/mobj_data.h
  ${CMAKE_CURRENT_SOURCE_DIR}/src/libbluray/hdmv/mobj_parse.c
  ${CMAKE_CURRENT_SOURCE_DIR}/src/libbluray/hdmv/mobj_parse.h
  ${CMAKE_CURRENT_SOURCE_DIR}/src/libbluray/hdmv/mobj_print.c
  ${CMAKE_CURRENT_SOURCE_DIR}/src/libbluray/hdmv/mobj_print.h
  ${CMAKE_CURRENT_SOURCE_DIR}/src/libbluray/keys.h
  ${CMAKE_CURRENT_SOURCE_DIR}/src/libbluray/player_settings.h
  ${CMAKE_CURRENT_SOURCE_DIR}/src/libbluray/register.c
  ${CMAKE_CURRENT_SOURCE_DIR}/src/libbluray/register.h
  ${CMAKE_CURRENT_SOURCE_DIR}/src/util/array.c
  ${CMAKE_CURRENT_SOURCE_DIR}/src/util/array.h
  ${CMAKE_CURRENT_SOURCE_DIR}/src/util/attributes.h
  ${CMAKE_CURRENT_SOURCE_DIR}/src/util/bits.c
  ${CMAKE_CURRENT_SOURCE_DIR}/src/util/bits.h
  ${CMAKE_CURRENT_SOURCE_DIR}/src/util/event_queue.c
  ${CMAKE_CURRENT_SOURCE_DIR}/src/util/event_queue.h
  ${CMAKE_CURRENT_SOURCE_DIR}/src/util/log_control.h
  ${CMAKE_CURRENT_SOURCE_DIR}/src/util/logging.c
  ${CMAKE_CURRENT_SOURCE_DIR}/src/util/logging.h
  ${CMAKE_CURRENT_SOURCE_DIR}/src/util/macro.h
  ${CMAKE_CURRENT_SOURCE_DIR}/src/util/mutex.c
  ${CMAKE_CURRENT_SOURCE_DIR}/src/util/mutex.h
  ${CMAKE_CURRENT_SOURCE_DIR}/src/util/refcnt.c
  ${CMAKE_CURRENT_SOURCE_DIR}/src/util/refcnt.h
  ${CMAKE_CURRENT_SOURCE_DIR}/src/util/strutl.c
  ${CMAKE_CURRENT_SOURCE_DIR}/src/util/strutl.h
  ${CMAKE_CURRENT_SOURCE_DIR}/src/util/time.c
  ${CMAKE_CURRENT_SOURCE_DIR}/src/util/time.h
)


# BD-J

if(NOT WINDOWS_STORE)
  find_package(JNI)
  if (JNI_FOUND)
    include_directories("${JAVA_INCLUDE_PATH}")
    include_directories("${JAVA_INCLUDE_PATH2}")
    set(HAVE_JNI_H 1)
  else()
    message( FATAL_ERROR "JNI not found." )
  endif()

  find_package(Java COMPONENTS Development)
  if (JAVA_FOUND)
    get_filename_component(_JDK_HOME "${Java_JAVA_EXECUTABLE}" PATH)
    get_filename_component(_JDK_HOME "${_JDK_HOME}/.." ABSOLUTE)
    message("JDK_HOME=${_JDK_HOME}")
  else()
    message( FATAL_ERROR "JNI not found." )
  endif()
endif(NOT WINDOWS_STORE)

add_library(libbluray SHARED ${SRCS})
target_link_libraries(libbluray PRIVATE freetype::freetype iconv::iconv libxml2::libxml2 libudfread::libudfread)
set_target_properties(libbluray
  PROPERTIES
    LINK_FLAGS "/DEF:\"libbluray.def\""
    PDB_OUTPUT_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
    PDB_NAME libbluray
    PDB_NAME_DEBUG libblurayd
)

target_compile_definitions(libbluray
  PRIVATE
    HAVE_CONFIG_H
    _CRT_SECURE_NO_WARNINGS
)
target_link_options(libbluray
  PRIVATE
    /INCREMENTAL:NO
    /debug:full
)
if(WINDOWS_STORE)
  target_compile_definitions(libbluray
    PRIVATE
      MS_APP
  )
endif()
target_include_directories(libbluray
  PRIVATE
  $<BUILD_INTERFACE:.;cmake;src;src/libbluray;${CMAKE_CURRENT_BINARY_DIR}>
  INTERFACE
  $<INSTALL_INTERFACE:include/libbluray>
)

if(NOT WINDOWS_STORE)
  target_link_libraries(libbluray
    PRIVATE
      shell32.lib
      advapi32.lib
      gdi32.lib
  )
  if (EXISTS $ENV{ANT_HOME})
  set(ANT_CMD $ENV{ANT_HOME}/bin/ant)
  set(ANT_BUILD ${CMAKE_CURRENT_SOURCE_DIR}/src/libbluray/bdj/build.xml)
    # STRING(REGEX REPLACE "/" "\\\\" ANT_BUILD "${CMAKE_CURRENT_SOURCE_DIR}/src/libbluray/bdj/build.xml")
    message("ANT command: ${ANT_CMD} -f ${ANT_BUILD}")
    add_custom_target (ANT
      COMMENT "compiling libbluray.jar"
      COMMAND
          ${ANT_CMD} -f "${ANT_BUILD}"
                    -Ddist="${CMAKE_CURRENT_BINARY_DIR}"
                    -Dsrc_awt=:java-j2se
                    -Dversion="j2se-${libbluray_VERSION}"
    )
    add_dependencies(libbluray ANT)
  else()
    message( FATAL_ERROR "ANT path not set, can't compile libbluray.jar")
  endif()
endif(NOT WINDOWS_STORE)

set(HAVE_FT2 1)
set(HAVE_LIBXML2 1)
CONFIGURE_FILE(${CMAKE_CURRENT_SOURCE_DIR}/config.h.cm ${CMAKE_CURRENT_BINARY_DIR}/config.h)

include(CMakePackageConfigHelpers)
write_basic_package_version_file(
  ${CMAKE_CURRENT_BINARY_DIR}/libbluray-config-version.cmake
  VERSION ${libbluray_VERSION}
  COMPATIBILITY AnyNewerVersion
)

install(TARGETS libbluray EXPORT libbluray
  RUNTIME DESTINATION bin
  ARCHIVE DESTINATION lib
  LIBRARY DESTINATION lib)


install(FILES
  ${CMAKE_CURRENT_SOURCE_DIR}/src/libbluray/bluray-version.h
  ${CMAKE_CURRENT_SOURCE_DIR}/src/file/filesystem.h
  ${CMAKE_CURRENT_SOURCE_DIR}/src/libbluray/bluray.h
  ${CMAKE_CURRENT_SOURCE_DIR}/src/libbluray/decoders/overlay.h
  ${CMAKE_CURRENT_SOURCE_DIR}/src/libbluray/keys.h
  ${CMAKE_CURRENT_SOURCE_DIR}/src/libbluray/player_settings.h
  ${CMAKE_CURRENT_SOURCE_DIR}/src/util/log_control.h
  ${CMAKE_CURRENT_SOURCE_DIR}/src/libbluray/bdnav/meta_data.h
  DESTINATION include/libbluray)

install(FILES
  $<TARGET_PDB_FILE:libbluray>
  DESTINATION lib
)

if(NOT WINDOWS_STORE)
  install(FILES
    ${CMAKE_CURRENT_BINARY_DIR}/libbluray-j2se-${libbluray_VERSION}.jar
    ${CMAKE_CURRENT_BINARY_DIR}/libbluray-awt-j2se-${libbluray_VERSION}.jar
    DESTINATION bin)
endif(NOT WINDOWS_STORE)

install(EXPORT libbluray
  FILE
    libbluray.cmake
  NAMESPACE
    libbluray::
  DESTINATION
    lib/cmake/libbluray
)
install(
  FILES
    cmake/libbluray-config.cmake
    ${CMAKE_CURRENT_BINARY_DIR}/libbluray-config-version.cmake
  DESTINATION
    lib/cmake/libbluray
)
