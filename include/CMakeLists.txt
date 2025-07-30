target_include_directories(
  skity
  PRIVATE
  ${CMAKE_CURRENT_LIST_DIR}
  ${CMAKE_CURRENT_BINARY_DIR}
)

set(SKITY_PUBLIC_HEADERS
  ${CMAKE_CURRENT_LIST_DIR}/skity/io/data.hpp
  ${CMAKE_CURRENT_LIST_DIR}/skity/io/logger.hpp
  ${CMAKE_CURRENT_LIST_DIR}/skity/io/pixmap.hpp
  ${CMAKE_CURRENT_LIST_DIR}/skity/effect/mask_filter.hpp
  ${CMAKE_CURRENT_LIST_DIR}/skity/effect/path_effect.hpp
  ${CMAKE_CURRENT_LIST_DIR}/skity/effect/shader.hpp
  ${CMAKE_CURRENT_LIST_DIR}/skity/geometry/point.hpp
  ${CMAKE_CURRENT_LIST_DIR}/skity/geometry/rect.hpp
  ${CMAKE_CURRENT_LIST_DIR}/skity/geometry/rrect.hpp
  ${CMAKE_CURRENT_LIST_DIR}/skity/geometry/scalar.hpp
  ${CMAKE_CURRENT_LIST_DIR}/skity/geometry/stroke.hpp
  ${CMAKE_CURRENT_LIST_DIR}/skity/geometry/vector.hpp
  ${CMAKE_CURRENT_LIST_DIR}/skity/gpu/gpu_backend_type.hpp
  ${CMAKE_CURRENT_LIST_DIR}/skity/gpu/gpu_context.hpp
  ${CMAKE_CURRENT_LIST_DIR}/skity/gpu/gpu_surface.hpp
  ${CMAKE_CURRENT_LIST_DIR}/skity/gpu/texture.hpp
  ${CMAKE_CURRENT_LIST_DIR}/skity/graphic/alpha_type.hpp
  ${CMAKE_CURRENT_LIST_DIR}/skity/graphic/bitmap.hpp
  ${CMAKE_CURRENT_LIST_DIR}/skity/graphic/color.hpp
  ${CMAKE_CURRENT_LIST_DIR}/skity/graphic/image.hpp
  ${CMAKE_CURRENT_LIST_DIR}/skity/graphic/paint.hpp
  ${CMAKE_CURRENT_LIST_DIR}/skity/graphic/path.hpp
  ${CMAKE_CURRENT_LIST_DIR}/skity/graphic/path_measure.hpp
  ${CMAKE_CURRENT_LIST_DIR}/skity/graphic/path_op.hpp
  ${CMAKE_CURRENT_LIST_DIR}/skity/graphic/sampling_options.hpp
  ${CMAKE_CURRENT_LIST_DIR}/skity/graphic/tile_mode.hpp
  ${CMAKE_CURRENT_LIST_DIR}/skity/macros.hpp
  ${CMAKE_CURRENT_LIST_DIR}/skity/render/canvas.hpp
  ${CMAKE_CURRENT_LIST_DIR}/skity/skity.hpp
  ${CMAKE_CURRENT_LIST_DIR}/skity/text/text_blob.hpp
  ${CMAKE_CURRENT_LIST_DIR}/skity/text/text_run.hpp
  ${CMAKE_CURRENT_LIST_DIR}/skity/text/typeface.hpp
  ${CMAKE_CURRENT_LIST_DIR}/skity/text/utf.hpp
  ${CMAKE_CURRENT_LIST_DIR}/skity/recorder/display_list.hpp
  ${CMAKE_CURRENT_LIST_DIR}/skity/recorder/picture_recorder.hpp
  ${CMAKE_CURRENT_LIST_DIR}/skity/recorder/recording_canvas.hpp
)

target_sources(skity PRIVATE ${SKITY_PUBLIC_HEADERS})


# install header
install(
  DIRECTORY
  "${CMAKE_CURRENT_LIST_DIR}/skity"
  DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
  FILES_MATCHING PATTERN "*.hpp"
)
