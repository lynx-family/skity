# Copyright 2021 The Lynx Authors. All rights reserved.
# Licensed under the Apache License Version 2.0 that can be found in the
# LICENSE file in the root directory of this source tree.

target_sources(
  skity
  PRIVATE
  ${CMAKE_CURRENT_LIST_DIR}/base/hash.cc
  ${CMAKE_CURRENT_LIST_DIR}/base/hash.hpp
  ${CMAKE_CURRENT_LIST_DIR}/base/lru_cache.hpp
  ${CMAKE_CURRENT_LIST_DIR}/base/mapping.cc
  ${CMAKE_CURRENT_LIST_DIR}/base/unique_fd.cc
  ${CMAKE_CURRENT_LIST_DIR}/effect/color_filter.cc
  ${CMAKE_CURRENT_LIST_DIR}/effect/color_filter_base.hpp
  ${CMAKE_CURRENT_LIST_DIR}/effect/dash_path_effect.cc
  ${CMAKE_CURRENT_LIST_DIR}/effect/dash_path_effect.hpp
  ${CMAKE_CURRENT_LIST_DIR}/effect/discrete_path_effect.cc
  ${CMAKE_CURRENT_LIST_DIR}/effect/discrete_path_effect.hpp
  ${CMAKE_CURRENT_LIST_DIR}/effect/gradient_shader.cc
  ${CMAKE_CURRENT_LIST_DIR}/effect/gradient_shader.hpp
  ${CMAKE_CURRENT_LIST_DIR}/effect/image_filter.cc
  ${CMAKE_CURRENT_LIST_DIR}/effect/mask_filter.cc
  ${CMAKE_CURRENT_LIST_DIR}/effect/path_effect.cc
  ${CMAKE_CURRENT_LIST_DIR}/effect/pixmap_shader.cc
  ${CMAKE_CURRENT_LIST_DIR}/effect/pixmap_shader.hpp
  ${CMAKE_CURRENT_LIST_DIR}/effect/shader.cc
  ${CMAKE_CURRENT_LIST_DIR}/geometry/camera.cc
  ${CMAKE_CURRENT_LIST_DIR}/geometry/conic.cc
  ${CMAKE_CURRENT_LIST_DIR}/geometry/conic.hpp
  ${CMAKE_CURRENT_LIST_DIR}/geometry/cubic.cc
  ${CMAKE_CURRENT_LIST_DIR}/geometry/cubic.hpp
  ${CMAKE_CURRENT_LIST_DIR}/geometry/geometry.cc
  ${CMAKE_CURRENT_LIST_DIR}/geometry/geometry.hpp
  ${CMAKE_CURRENT_LIST_DIR}/geometry/glm_helper.hpp
  ${CMAKE_CURRENT_LIST_DIR}/geometry/math.hpp
  ${CMAKE_CURRENT_LIST_DIR}/geometry/matrix.cc
  ${CMAKE_CURRENT_LIST_DIR}/geometry/quaternion.cc
  ${CMAKE_CURRENT_LIST_DIR}/geometry/point_priv.hpp
  ${CMAKE_CURRENT_LIST_DIR}/geometry/rect.cc
  ${CMAKE_CURRENT_LIST_DIR}/geometry/rrect.cc
  ${CMAKE_CURRENT_LIST_DIR}/geometry/stroke.cc
  ${CMAKE_CURRENT_LIST_DIR}/graphic/bitmap_sampler.cc
  ${CMAKE_CURRENT_LIST_DIR}/graphic/bitmap_sampler.hpp
  ${CMAKE_CURRENT_LIST_DIR}/graphic/bitmap.cc
  ${CMAKE_CURRENT_LIST_DIR}/graphic/blend_mode.cc
  ${CMAKE_CURRENT_LIST_DIR}/graphic/blend_mode_priv.hpp
  ${CMAKE_CURRENT_LIST_DIR}/graphic/color.cc
  ${CMAKE_CURRENT_LIST_DIR}/graphic/color_priv.cc
  ${CMAKE_CURRENT_LIST_DIR}/graphic/color_priv.hpp
  ${CMAKE_CURRENT_LIST_DIR}/graphic/color_priv_neon.hpp
  ${CMAKE_CURRENT_LIST_DIR}/graphic/contour_measure.cc
  ${CMAKE_CURRENT_LIST_DIR}/graphic/contour_measure.hpp
  ${CMAKE_CURRENT_LIST_DIR}/graphic/image.cc
  ${CMAKE_CURRENT_LIST_DIR}/graphic/paint.cc
  ${CMAKE_CURRENT_LIST_DIR}/graphic/path.cc
  ${CMAKE_CURRENT_LIST_DIR}/graphic/path_measure.cc
  ${CMAKE_CURRENT_LIST_DIR}/graphic/path_op.cc
  ${CMAKE_CURRENT_LIST_DIR}/graphic/path_priv.cc
  ${CMAKE_CURRENT_LIST_DIR}/graphic/path_priv.hpp
  ${CMAKE_CURRENT_LIST_DIR}/graphic/path_scanner.cc
  ${CMAKE_CURRENT_LIST_DIR}/graphic/path_scanner.hpp
  ${CMAKE_CURRENT_LIST_DIR}/graphic/path_visitor.cc
  ${CMAKE_CURRENT_LIST_DIR}/graphic/path_visitor.hpp
  ${CMAKE_CURRENT_LIST_DIR}/graphic/pathop/clipper2/core.h
  ${CMAKE_CURRENT_LIST_DIR}/graphic/pathop/clipper2/engine.cc
  ${CMAKE_CURRENT_LIST_DIR}/graphic/pathop/clipper2/engine.h
  ${CMAKE_CURRENT_LIST_DIR}/graphic/pathop/path_op_engine.cc
  ${CMAKE_CURRENT_LIST_DIR}/graphic/pathop/path_op_engine.hpp
  ${CMAKE_CURRENT_LIST_DIR}/io/data.cc
  ${CMAKE_CURRENT_LIST_DIR}/io/pixmap.cc
  ${CMAKE_CURRENT_LIST_DIR}/logging.cc
  ${CMAKE_CURRENT_LIST_DIR}/logging.hpp
  ${CMAKE_CURRENT_LIST_DIR}/tracing.cc
  ${CMAKE_CURRENT_LIST_DIR}/tracing.hpp
  ${CMAKE_CURRENT_LIST_DIR}/recorder/display_list_builder.hpp
  ${CMAKE_CURRENT_LIST_DIR}/recorder/display_list.cc
  ${CMAKE_CURRENT_LIST_DIR}/recorder/picture_recorder.cc
  ${CMAKE_CURRENT_LIST_DIR}/recorder/recorded_op.hpp
  ${CMAKE_CURRENT_LIST_DIR}/recorder/recording_canvas.cc
  ${CMAKE_CURRENT_LIST_DIR}/render/canvas.cc
  ${CMAKE_CURRENT_LIST_DIR}/render/canvas_state.cc
  ${CMAKE_CURRENT_LIST_DIR}/render/canvas_state.hpp
  ${CMAKE_CURRENT_LIST_DIR}/render/text/atlas/atlas_allocator.cc
  ${CMAKE_CURRENT_LIST_DIR}/render/text/atlas/atlas_allocator.hpp
  ${CMAKE_CURRENT_LIST_DIR}/render/text/atlas/atlas_bitmap.cc
  ${CMAKE_CURRENT_LIST_DIR}/render/text/atlas/atlas_bitmap.hpp
  ${CMAKE_CURRENT_LIST_DIR}/render/text/atlas/atlas_glyph.hpp
  ${CMAKE_CURRENT_LIST_DIR}/render/text/atlas/atlas_manager.cc
  ${CMAKE_CURRENT_LIST_DIR}/render/text/atlas/atlas_manager.hpp
  ${CMAKE_CURRENT_LIST_DIR}/render/text/atlas/atlas_texture.cc
  ${CMAKE_CURRENT_LIST_DIR}/render/text/atlas/atlas_texture.hpp
  ${CMAKE_CURRENT_LIST_DIR}/render/text/glyph_run.cc
  ${CMAKE_CURRENT_LIST_DIR}/render/text/glyph_run.hpp
  ${CMAKE_CURRENT_LIST_DIR}/render/text/sdf_gen.cc
  ${CMAKE_CURRENT_LIST_DIR}/render/text/sdf_gen.hpp
  ${CMAKE_CURRENT_LIST_DIR}/render/text/text_render_control.cc
  ${CMAKE_CURRENT_LIST_DIR}/render/text/text_render_control.hpp
  ${CMAKE_CURRENT_LIST_DIR}/render/text/text_transform.cc
  ${CMAKE_CURRENT_LIST_DIR}/render/text/text_transform.hpp
  ${CMAKE_CURRENT_LIST_DIR}/text/font.cc
  ${CMAKE_CURRENT_LIST_DIR}/text/font_style.cc
  ${CMAKE_CURRENT_LIST_DIR}/text/font_manager.cc
  ${CMAKE_CURRENT_LIST_DIR}/text/scaler_context_cache.cc
  ${CMAKE_CURRENT_LIST_DIR}/text/scaler_context_container.cc
  ${CMAKE_CURRENT_LIST_DIR}/text/scaler_context_desc.cc
  ${CMAKE_CURRENT_LIST_DIR}/text/scaler_context.cc
  ${CMAKE_CURRENT_LIST_DIR}/text/text_blob.cc
  ${CMAKE_CURRENT_LIST_DIR}/text/text_run.cc
  ${CMAKE_CURRENT_LIST_DIR}/text/typeface.cc
  ${CMAKE_CURRENT_LIST_DIR}/text/utf.cc
  ${CMAKE_CURRENT_LIST_DIR}/utils/xml/xml_parser.cc
  ${CMAKE_CURRENT_LIST_DIR}/utils/annotation_mutex.hpp
  ${CMAKE_CURRENT_LIST_DIR}/utils/arena_allocator.cc
  ${CMAKE_CURRENT_LIST_DIR}/utils/arena_allocator.hpp
  ${CMAKE_CURRENT_LIST_DIR}/utils/array_list.hpp
  ${CMAKE_CURRENT_LIST_DIR}/utils/list.hpp
  ${CMAKE_CURRENT_LIST_DIR}/utils/settings.cc
  ${CMAKE_CURRENT_LIST_DIR}/utils/thread_annotations.hpp
  ${CMAKE_CURRENT_LIST_DIR}/utils/unique_id.cc
  ${CMAKE_CURRENT_LIST_DIR}/utils/unique_id.hpp
  ${CMAKE_CURRENT_LIST_DIR}/utils/vector_cache.hpp
)

if(CMAKE_SYSTEM_NAME MATCHES "Windows")
  target_sources(
    skity
    PRIVATE
    ${CMAKE_CURRENT_LIST_DIR}/base/platform/win/file_win.cc
    ${CMAKE_CURRENT_LIST_DIR}/base/platform/win/handle_result.cc
    ${CMAKE_CURRENT_LIST_DIR}/base/platform/win/mapping_win.cc
  )
  target_link_libraries(skity PRIVATE Shlwapi)
else()
  target_sources(
    skity
    PRIVATE
    ${CMAKE_CURRENT_LIST_DIR}/base/platform/posix/file_posix.cc
    ${CMAKE_CURRENT_LIST_DIR}/base/platform/posix/mapping_posix.cc
  )
endif()

# text rendering
add_definitions(-DENABLE_TEXT_RENDER=1)
add_definitions(-D_SILENCE_CXX17_CODECVT_HEADER_DEPRECATION_WARNING)

if (NOT ${SKITY_CT_FONT})

  set(FREETYPE_DIR ${CMAKE_CURRENT_LIST_DIR}/../third_party/freetype2)
  set(ROOT_DIR ${CMAKE_CURRENT_LIST_DIR}/../)
  message("Freetype Dir")
  message(${FREETYPE_DIR})
  message("current path ${CMAKE_CURRENT_LIST_DIR}")
  include(${FREETYPE_DIR}/CMakeLists-Skity.txt)

  target_link_libraries(skity PRIVATE freetype2)

  if (NOT USE_THIRD_PARTY_ZLIB)
    # link system zlib
    target_link_libraries(skity PRIVATE z)
  endif()

  target_include_directories(skity PRIVATE ${FREETYPE_DIR}/include)

  # png needed by freetype2 from lynx
  target_include_directories(skity PRIVATE ${CMAKE_CURRENT_LIST_DIR}/../third_party/nanopng/include)
  target_sources(skity PRIVATE ${CMAKE_CURRENT_LIST_DIR}/../third_party/nanopng/src/nanopng16.cc)

  target_sources(
    skity
    PRIVATE
    ${CMAKE_CURRENT_LIST_DIR}/text/ports/color_freetype.cc
    ${CMAKE_CURRENT_LIST_DIR}/text/ports/color_freetype.hpp
    ${CMAKE_CURRENT_LIST_DIR}/text/ports/freetype_face.cc
    ${CMAKE_CURRENT_LIST_DIR}/text/ports/freetype_face.hpp
    ${CMAKE_CURRENT_LIST_DIR}/text/ports/path_freetype.cc
    ${CMAKE_CURRENT_LIST_DIR}/text/ports/path_freetype.hpp
    ${CMAKE_CURRENT_LIST_DIR}/text/ports/scaler_context_freetype.cc
    ${CMAKE_CURRENT_LIST_DIR}/text/ports/scaler_context_freetype.hpp
    ${CMAKE_CURRENT_LIST_DIR}/text/ports/typeface_freetype.cc
    ${CMAKE_CURRENT_LIST_DIR}/text/ports/typeface_freetype.hpp
  )

endif()
if(CMAKE_SYSTEM_NAME STREQUAL "Android")
  # header file for functor view
  target_include_directories(skity
    PRIVATE
    ${CMAKE_CURRENT_LIST_DIR}/../third_party/chromium
  )

  target_sources(
    skity
    PRIVATE
    ${CMAKE_CURRENT_LIST_DIR}/text/ports/android/ndk_font_api.cc
    ${CMAKE_CURRENT_LIST_DIR}/text/ports/android_fonts_parser.cc
    ${CMAKE_CURRENT_LIST_DIR}/text/ports/android_fonts_parser.hpp
    ${CMAKE_CURRENT_LIST_DIR}/text/ports/font_manager_android.cc
  )
elseif(CMAKE_SYSTEM_NAME STREQUAL "Darwin" AND SKITY_CT_FONT)
  target_sources(
    skity
    PRIVATE
    ${CMAKE_CURRENT_LIST_DIR}/text/ports/darwin/font_manager_darwin.cc
    ${CMAKE_CURRENT_LIST_DIR}/text/ports/darwin/font_manager_darwin.hpp
    ${CMAKE_CURRENT_LIST_DIR}/text/ports/darwin/scaler_context_darwin.cc
    ${CMAKE_CURRENT_LIST_DIR}/text/ports/darwin/scaler_context_darwin.hpp
    ${CMAKE_CURRENT_LIST_DIR}/text/ports/darwin/typeface_darwin.cc
    ${CMAKE_CURRENT_LIST_DIR}/text/ports/darwin/typeface_darwin.hpp
    ${CMAKE_CURRENT_LIST_DIR}/text/ports/darwin/types_darwin.cc
    ${CMAKE_CURRENT_LIST_DIR}/text/ports/darwin/types_darwin.hpp
  )

  target_link_libraries(skity PRIVATE "-framework CoreText" "-framework CoreGraphics")
elseif(CMAKE_SYSTEM_NAME STREQUAL "OHOS")
  target_sources(
    skity
    PRIVATE
    ${CMAKE_CURRENT_LIST_DIR}/text/ports/harmony/harmony_fonts_parser.cc
    ${CMAKE_CURRENT_LIST_DIR}/text/ports/harmony/harmony_fonts_parser.hpp
    ${CMAKE_CURRENT_LIST_DIR}/text/ports/harmony/font_manager_harmony.cc
  )
elseif(CMAKE_SYSTEM_NAME MATCHES "Windows")
  target_sources(
    skity
    PRIVATE
    ${CMAKE_CURRENT_LIST_DIR}/text/ports/win/font_manager_win.cc
  )
elseif(CMAKE_SYSTEM_NAME STREQUAL "Linux" OR CMAKE_SYSTEM_NAME STREQUAL "Darwin")
  target_sources(skity PRIVATE ${CMAKE_CURRENT_LIST_DIR}/text/ports/font_manager_freetype.cc)
else()
  message(STATUS "typeface empty system: ${CMAKE_SYSTEM_NAME}")
  target_sources(
    skity
    PRIVATE
    ${CMAKE_CURRENT_LIST_DIR}/text/ports/font_manager_empty.cc
    ${CMAKE_CURRENT_LIST_DIR}/text/ports/typeface_empty.cc
    ${CMAKE_CURRENT_LIST_DIR}/text/ports/typeface_empty.hpp
  )
endif()

# hardware backends
if(${SKITY_HW_RENDERER})
  target_sources(
    skity
    PRIVATE
    ${CMAKE_CURRENT_LIST_DIR}/gpu/backend_cast.hpp
    ${CMAKE_CURRENT_LIST_DIR}/gpu/gpu_buffer.hpp
    ${CMAKE_CURRENT_LIST_DIR}/gpu/gpu_device.cc
    ${CMAKE_CURRENT_LIST_DIR}/gpu/gpu_device.hpp
    ${CMAKE_CURRENT_LIST_DIR}/gpu/gpu_context_impl.cc
    ${CMAKE_CURRENT_LIST_DIR}/gpu/gpu_context_impl.hpp
    ${CMAKE_CURRENT_LIST_DIR}/gpu/gpu_render_pipeline.cc
    ${CMAKE_CURRENT_LIST_DIR}/gpu/gpu_render_pipeline.hpp
    ${CMAKE_CURRENT_LIST_DIR}/gpu/gpu_render_target.cc
    ${CMAKE_CURRENT_LIST_DIR}/gpu/gpu_sampler.cc
    ${CMAKE_CURRENT_LIST_DIR}/gpu/gpu_sampler.hpp
    ${CMAKE_CURRENT_LIST_DIR}/gpu/gpu_shader_function.hpp
    ${CMAKE_CURRENT_LIST_DIR}/gpu/gpu_surface_impl.cc
    ${CMAKE_CURRENT_LIST_DIR}/gpu/gpu_surface_impl.hpp
    ${CMAKE_CURRENT_LIST_DIR}/gpu/gpu_texture.cc
    ${CMAKE_CURRENT_LIST_DIR}/gpu/gpu_texture.hpp
    ${CMAKE_CURRENT_LIST_DIR}/gpu/texture_impl.cc
    ${CMAKE_CURRENT_LIST_DIR}/gpu/texture_impl.hpp
    ${CMAKE_CURRENT_LIST_DIR}/gpu/texture_manager.cc
    ${CMAKE_CURRENT_LIST_DIR}/gpu/texture_manager.hpp
    ${CMAKE_CURRENT_LIST_DIR}/render/hw/filters/hw_blur_filter.cc
    ${CMAKE_CURRENT_LIST_DIR}/render/hw/filters/hw_blur_filter.hpp
    ${CMAKE_CURRENT_LIST_DIR}/render/hw/filters/hw_color_filter.cc
    ${CMAKE_CURRENT_LIST_DIR}/render/hw/filters/hw_color_filter.hpp
    ${CMAKE_CURRENT_LIST_DIR}/render/hw/filters/hw_down_sampler_filter.cc
    ${CMAKE_CURRENT_LIST_DIR}/render/hw/filters/hw_down_sampler_filter.hpp
    ${CMAKE_CURRENT_LIST_DIR}/render/hw/filters/hw_filter.cc
    ${CMAKE_CURRENT_LIST_DIR}/render/hw/filters/hw_filter.hpp
    ${CMAKE_CURRENT_LIST_DIR}/render/hw/filters/hw_filters.cc
    ${CMAKE_CURRENT_LIST_DIR}/render/hw/filters/hw_filters.hpp
    ${CMAKE_CURRENT_LIST_DIR}/render/hw/filters/hw_matrix_filter.cc
    ${CMAKE_CURRENT_LIST_DIR}/render/hw/filters/hw_matrix_filter.hpp
    ${CMAKE_CURRENT_LIST_DIR}/render/hw/filters/hw_merge_filter.cc
    ${CMAKE_CURRENT_LIST_DIR}/render/hw/filters/hw_merge_filter.hpp
    ${CMAKE_CURRENT_LIST_DIR}/render/hw/layer/hw_filter_layer.cc
    ${CMAKE_CURRENT_LIST_DIR}/render/hw/layer/hw_filter_layer.hpp
    ${CMAKE_CURRENT_LIST_DIR}/render/hw/layer/hw_root_layer.cc
    ${CMAKE_CURRENT_LIST_DIR}/render/hw/layer/hw_root_layer.hpp
    ${CMAKE_CURRENT_LIST_DIR}/render/hw/layer/hw_sub_layer.cc
    ${CMAKE_CURRENT_LIST_DIR}/render/hw/layer/hw_sub_layer.hpp
    ${CMAKE_CURRENT_LIST_DIR}/render/hw/hw_canvas.cc
    ${CMAKE_CURRENT_LIST_DIR}/render/hw/hw_canvas.hpp
    ${CMAKE_CURRENT_LIST_DIR}/render/hw/hw_draw.cc
    ${CMAKE_CURRENT_LIST_DIR}/render/hw/hw_draw.hpp
    ${CMAKE_CURRENT_LIST_DIR}/render/hw/hw_geometry_raster.cc
    ${CMAKE_CURRENT_LIST_DIR}/render/hw/hw_geometry_raster.hpp
    ${CMAKE_CURRENT_LIST_DIR}/render/hw/hw_layer.cc
    ${CMAKE_CURRENT_LIST_DIR}/render/hw/hw_layer.hpp
    ${CMAKE_CURRENT_LIST_DIR}/render/hw/hw_layer_state.cc
    ${CMAKE_CURRENT_LIST_DIR}/render/hw/hw_layer_state.hpp
    ${CMAKE_CURRENT_LIST_DIR}/render/hw/hw_path_aa_outline.cc
    ${CMAKE_CURRENT_LIST_DIR}/render/hw/hw_path_aa_outline.hpp
    ${CMAKE_CURRENT_LIST_DIR}/render/hw/hw_path_raster.cc
    ${CMAKE_CURRENT_LIST_DIR}/render/hw/hw_path_raster.hpp
    ${CMAKE_CURRENT_LIST_DIR}/render/hw/hw_path_visitor.cc
    ${CMAKE_CURRENT_LIST_DIR}/render/hw/hw_path_visitor.hpp
    ${CMAKE_CURRENT_LIST_DIR}/render/hw/hw_pipeline_lib.cc
    ${CMAKE_CURRENT_LIST_DIR}/render/hw/hw_pipeline_lib.hpp
    ${CMAKE_CURRENT_LIST_DIR}/render/hw/hw_render_pass_builder.cc
    ${CMAKE_CURRENT_LIST_DIR}/render/hw/hw_render_pass_builder.hpp
    ${CMAKE_CURRENT_LIST_DIR}/render/hw/hw_render_target_cache.hpp
    ${CMAKE_CURRENT_LIST_DIR}/render/hw/hw_resource_cache.hpp
    ${CMAKE_CURRENT_LIST_DIR}/render/hw/hw_stage_buffer.cc
    ${CMAKE_CURRENT_LIST_DIR}/render/hw/hw_stage_buffer.hpp
    ${CMAKE_CURRENT_LIST_DIR}/render/hw/hw_static_buffer.cc
    ${CMAKE_CURRENT_LIST_DIR}/render/hw/hw_static_buffer.hpp
  )

  # Dynamic shader
  target_sources(
    skity
    PRIVATE
    ${CMAKE_CURRENT_LIST_DIR}/gpu/gpu_shader_module.cc
    ${CMAKE_CURRENT_LIST_DIR}/gpu/gpu_shader_module.hpp
    ${CMAKE_CURRENT_LIST_DIR}/render/hw/draw/fragment/wgsl_blur_filter.cc
    ${CMAKE_CURRENT_LIST_DIR}/render/hw/draw/fragment/wgsl_blur_filter.hpp
    ${CMAKE_CURRENT_LIST_DIR}/render/hw/draw/fragment/wgsl_gradient_fragment.cc
    ${CMAKE_CURRENT_LIST_DIR}/render/hw/draw/fragment/wgsl_gradient_fragment.hpp
    ${CMAKE_CURRENT_LIST_DIR}/render/hw/draw/fragment/wgsl_image_filter.cc
    ${CMAKE_CURRENT_LIST_DIR}/render/hw/draw/fragment/wgsl_image_filter.hpp
    ${CMAKE_CURRENT_LIST_DIR}/render/hw/draw/fragment/wgsl_solid_color.cc
    ${CMAKE_CURRENT_LIST_DIR}/render/hw/draw/fragment/wgsl_solid_color.hpp
    ${CMAKE_CURRENT_LIST_DIR}/render/hw/draw/fragment/wgsl_stencil_fragment.cc
    ${CMAKE_CURRENT_LIST_DIR}/render/hw/draw/fragment/wgsl_stencil_fragment.hpp
    ${CMAKE_CURRENT_LIST_DIR}/render/hw/draw/fragment/wgsl_text_fragment.cc
    ${CMAKE_CURRENT_LIST_DIR}/render/hw/draw/fragment/wgsl_text_fragment.hpp
    ${CMAKE_CURRENT_LIST_DIR}/render/hw/draw/fragment/wgsl_texture_fragment.cc
    ${CMAKE_CURRENT_LIST_DIR}/render/hw/draw/fragment/wgsl_texture_fragment.hpp
    ${CMAKE_CURRENT_LIST_DIR}/render/hw/draw/geometry/wgsl_clip_geometry.cc
    ${CMAKE_CURRENT_LIST_DIR}/render/hw/draw/geometry/wgsl_clip_geometry.hpp
    ${CMAKE_CURRENT_LIST_DIR}/render/hw/draw/geometry/wgsl_filter_geometry.cc
    ${CMAKE_CURRENT_LIST_DIR}/render/hw/draw/geometry/wgsl_filter_geometry.hpp
    ${CMAKE_CURRENT_LIST_DIR}/render/hw/draw/geometry/wgsl_gradient_path.cc
    ${CMAKE_CURRENT_LIST_DIR}/render/hw/draw/geometry/wgsl_gradient_path.hpp
    ${CMAKE_CURRENT_LIST_DIR}/render/hw/draw/geometry/wgsl_path_geometry.cc
    ${CMAKE_CURRENT_LIST_DIR}/render/hw/draw/geometry/wgsl_path_geometry.hpp
    ${CMAKE_CURRENT_LIST_DIR}/render/hw/draw/geometry/wgsl_tess_path_fill_geometry.cc
    ${CMAKE_CURRENT_LIST_DIR}/render/hw/draw/geometry/wgsl_tess_path_fill_geometry.hpp
    ${CMAKE_CURRENT_LIST_DIR}/render/hw/draw/geometry/wgsl_tess_path_stroke_geometry.cc
    ${CMAKE_CURRENT_LIST_DIR}/render/hw/draw/geometry/wgsl_tess_path_stroke_geometry.hpp
    ${CMAKE_CURRENT_LIST_DIR}/render/hw/draw/geometry/wgsl_text_geometry.cc
    ${CMAKE_CURRENT_LIST_DIR}/render/hw/draw/geometry/wgsl_text_geometry.hpp
    ${CMAKE_CURRENT_LIST_DIR}/render/hw/draw/geometry/wgsl_texture_path.cc
    ${CMAKE_CURRENT_LIST_DIR}/render/hw/draw/geometry/wgsl_texture_path.hpp
    ${CMAKE_CURRENT_LIST_DIR}/render/hw/draw/hw_draw_step.cc
    ${CMAKE_CURRENT_LIST_DIR}/render/hw/draw/hw_draw_step.hpp
    ${CMAKE_CURRENT_LIST_DIR}/render/hw/draw/hw_dynamic_draw.cc
    ${CMAKE_CURRENT_LIST_DIR}/render/hw/draw/hw_dynamic_draw.hpp
    ${CMAKE_CURRENT_LIST_DIR}/render/hw/draw/hw_dynamic_path_clip.cc
    ${CMAKE_CURRENT_LIST_DIR}/render/hw/draw/hw_dynamic_path_clip.hpp
    ${CMAKE_CURRENT_LIST_DIR}/render/hw/draw/hw_dynamic_path_draw.cc
    ${CMAKE_CURRENT_LIST_DIR}/render/hw/draw/hw_dynamic_path_draw.hpp
    ${CMAKE_CURRENT_LIST_DIR}/render/hw/draw/hw_dynamic_text_draw.cc
    ${CMAKE_CURRENT_LIST_DIR}/render/hw/draw/hw_dynamic_text_draw.hpp
    ${CMAKE_CURRENT_LIST_DIR}/render/hw/draw/hw_wgsl_fragment.hpp
    ${CMAKE_CURRENT_LIST_DIR}/render/hw/draw/hw_wgsl_geometry.hpp
    ${CMAKE_CURRENT_LIST_DIR}/render/hw/draw/step/clip_step.cc
    ${CMAKE_CURRENT_LIST_DIR}/render/hw/draw/step/clip_step.hpp
    ${CMAKE_CURRENT_LIST_DIR}/render/hw/draw/step/color_step.cc
    ${CMAKE_CURRENT_LIST_DIR}/render/hw/draw/step/color_step.hpp
    ${CMAKE_CURRENT_LIST_DIR}/render/hw/draw/step/stencil_step.cc
    ${CMAKE_CURRENT_LIST_DIR}/render/hw/draw/step/stencil_step.hpp
    ${CMAKE_CURRENT_LIST_DIR}/render/hw/draw/wgx_filter.cc
    ${CMAKE_CURRENT_LIST_DIR}/render/hw/draw/wgx_filter.hpp
    ${CMAKE_CURRENT_LIST_DIR}/render/hw/draw/wgx_utils.cc
    ${CMAKE_CURRENT_LIST_DIR}/render/hw/draw/wgx_utils.hpp
  )

  target_link_libraries(skity PRIVATE wgsl-cross)

  # OpenGL backends
  if(${SKITY_GL_BACKEND})

    target_include_directories(skity PRIVATE ${CMAKE_CURRENT_LIST_DIR}/../third_party/glad/include)

    target_sources(
      skity
      PRIVATE
      ${CMAKE_CURRENT_LIST_DIR}/gpu/gl/gl_interface.cc
      ${CMAKE_CURRENT_LIST_DIR}/gpu/gl/gl_interface.hpp
      ${CMAKE_CURRENT_LIST_DIR}/gpu/gl/gpu_blit_pass_gl.cc
      ${CMAKE_CURRENT_LIST_DIR}/gpu/gl/gpu_blit_pass_gl.hpp
      ${CMAKE_CURRENT_LIST_DIR}/gpu/gl/gpu_buffer_gl.cc
      ${CMAKE_CURRENT_LIST_DIR}/gpu/gl/gpu_buffer_gl.hpp
      ${CMAKE_CURRENT_LIST_DIR}/gpu/gl/gpu_command_buffer_gl.cc
      ${CMAKE_CURRENT_LIST_DIR}/gpu/gl/gpu_command_buffer_gl.hpp
      ${CMAKE_CURRENT_LIST_DIR}/gpu/gl/gpu_device_gl.cc
      ${CMAKE_CURRENT_LIST_DIR}/gpu/gl/gpu_device_gl.hpp
      ${CMAKE_CURRENT_LIST_DIR}/gpu/gl/gpu_context_impl_gl.cc
      ${CMAKE_CURRENT_LIST_DIR}/gpu/gl/gpu_context_impl_gl.hpp
      ${CMAKE_CURRENT_LIST_DIR}/gpu/gl/gpu_render_pass_gl.cc
      ${CMAKE_CURRENT_LIST_DIR}/gpu/gl/gpu_render_pass_gl.hpp
      ${CMAKE_CURRENT_LIST_DIR}/gpu/gl/gpu_render_pipeline_gl.cc
      ${CMAKE_CURRENT_LIST_DIR}/gpu/gl/gpu_render_pipeline_gl.hpp
      ${CMAKE_CURRENT_LIST_DIR}/gpu/gl/gpu_sampler_gl.cc
      ${CMAKE_CURRENT_LIST_DIR}/gpu/gl/gpu_sampler_gl.hpp
      ${CMAKE_CURRENT_LIST_DIR}/gpu/gl/gpu_shader_function_gl.cc
      ${CMAKE_CURRENT_LIST_DIR}/gpu/gl/gpu_shader_function_gl.hpp
      ${CMAKE_CURRENT_LIST_DIR}/gpu/gl/gpu_surface_gl.cc
      ${CMAKE_CURRENT_LIST_DIR}/gpu/gl/gpu_surface_gl.hpp
      ${CMAKE_CURRENT_LIST_DIR}/gpu/gl/gpu_texture_gl.cc
      ${CMAKE_CURRENT_LIST_DIR}/gpu/gl/gpu_texture_gl.hpp
      ${CMAKE_CURRENT_LIST_DIR}/render/hw/gl/gl_root_layer.cc
      ${CMAKE_CURRENT_LIST_DIR}/render/hw/gl/gl_root_layer.hpp
    )

    if(${SKITY_USE_MESA_GLES})
      target_compile_definitions(skity PRIVATE -DSKITY_MESA_GLES)
    endif()
  endif()

  # Metal backend
  if(${SKITY_MTL_BACKEND})
    target_sources(
      skity
      PRIVATE
      ${CMAKE_CURRENT_LIST_DIR}/gpu/mtl/formats_mtl.h
      ${CMAKE_CURRENT_LIST_DIR}/gpu/mtl/formats_mtl.mm
      ${CMAKE_CURRENT_LIST_DIR}/gpu/mtl/gpu_blit_pass_mtl.h
      ${CMAKE_CURRENT_LIST_DIR}/gpu/mtl/gpu_blit_pass_mtl.mm
      ${CMAKE_CURRENT_LIST_DIR}/gpu/mtl/gpu_buffer_mtl.h
      ${CMAKE_CURRENT_LIST_DIR}/gpu/mtl/gpu_buffer_mtl.mm
      ${CMAKE_CURRENT_LIST_DIR}/gpu/mtl/gpu_command_buffer_mtl.h
      ${CMAKE_CURRENT_LIST_DIR}/gpu/mtl/gpu_command_buffer_mtl.mm
      ${CMAKE_CURRENT_LIST_DIR}/gpu/mtl/gpu_context_impl_mtl.h
      ${CMAKE_CURRENT_LIST_DIR}/gpu/mtl/gpu_context_impl_mtl.mm
      ${CMAKE_CURRENT_LIST_DIR}/gpu/mtl/gpu_device_mtl.h
      ${CMAKE_CURRENT_LIST_DIR}/gpu/mtl/gpu_device_mtl.mm
      ${CMAKE_CURRENT_LIST_DIR}/gpu/mtl/gpu_render_pass_mtl.h
      ${CMAKE_CURRENT_LIST_DIR}/gpu/mtl/gpu_render_pass_mtl.mm
      ${CMAKE_CURRENT_LIST_DIR}/gpu/mtl/gpu_render_pipeline_mtl.h
      ${CMAKE_CURRENT_LIST_DIR}/gpu/mtl/gpu_render_pipeline_mtl.mm
      ${CMAKE_CURRENT_LIST_DIR}/gpu/mtl/gpu_sampler_mtl.h
      ${CMAKE_CURRENT_LIST_DIR}/gpu/mtl/gpu_sampler_mtl.mm
      ${CMAKE_CURRENT_LIST_DIR}/gpu/mtl/gpu_shader_function_mtl.h
      ${CMAKE_CURRENT_LIST_DIR}/gpu/mtl/gpu_shader_function_mtl.mm
      ${CMAKE_CURRENT_LIST_DIR}/gpu/mtl/gpu_surface_mtl.h
      ${CMAKE_CURRENT_LIST_DIR}/gpu/mtl/gpu_surface_mtl.mm
      ${CMAKE_CURRENT_LIST_DIR}/gpu/mtl/gpu_texture_mtl.h
      ${CMAKE_CURRENT_LIST_DIR}/gpu/mtl/gpu_texture_mtl.mm
      ${CMAKE_CURRENT_LIST_DIR}/render/hw/mtl/mtl_root_layer.h
      ${CMAKE_CURRENT_LIST_DIR}/render/hw/mtl/mtl_root_layer.mm
    )

    target_link_libraries(skity PUBLIC
      "-framework Metal"
      "-framework Foundation"
    )
  endif()
endif()

if(${SKITY_SW_RENDERER})
  target_sources(
    skity
    PRIVATE
    ${CMAKE_CURRENT_LIST_DIR}/render/sw/sw_a8_drawable.cc
    ${CMAKE_CURRENT_LIST_DIR}/render/sw/sw_a8_drawable.hpp
    ${CMAKE_CURRENT_LIST_DIR}/render/sw/sw_canvas.cc
    ${CMAKE_CURRENT_LIST_DIR}/render/sw/sw_canvas.hpp
    ${CMAKE_CURRENT_LIST_DIR}/render/sw/sw_edge.cc
    ${CMAKE_CURRENT_LIST_DIR}/render/sw/sw_edge.hpp
    ${CMAKE_CURRENT_LIST_DIR}/render/sw/sw_raster.cc
    ${CMAKE_CURRENT_LIST_DIR}/render/sw/sw_raster.hpp
    ${CMAKE_CURRENT_LIST_DIR}/render/sw/sw_render_target.cc
    ${CMAKE_CURRENT_LIST_DIR}/render/sw/sw_render_target.hpp
    ${CMAKE_CURRENT_LIST_DIR}/render/sw/sw_span_brush.cc
    ${CMAKE_CURRENT_LIST_DIR}/render/sw/sw_span_brush.hpp
    ${CMAKE_CURRENT_LIST_DIR}/render/sw/sw_stack_blur.cc
    ${CMAKE_CURRENT_LIST_DIR}/render/sw/sw_stack_blur.hpp
    ${CMAKE_CURRENT_LIST_DIR}/render/sw/sw_subpixel.hpp
  )
endif()

if(EMSCRIPTEN)
  target_sources(skity PRIVATE ${CMAKE_CURRENT_LIST_DIR}/render/wasm/wasm_canvas.cc)
  add_executable(skity_wasm ${CMAKE_CURRENT_LIST_DIR}/render/wasm/wasm_binding.cc)
  target_compile_options(skity_wasm PUBLIC "-s" "USE_FREETYPE=1")
  target_link_libraries(skity_wasm skity::skity)

  # output dir
  set_target_properties(skity_wasm PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${PROJECT_BINARY_DIR})
  set_target_properties(skity_wasm PROPERTIES LINK_FLAGS "-s WASM=1 -sSINGLE_FILE -sMAX_WEBGL_VERSION=2 -sFULL_ES3 -s MODULARIZE=1 -sGL_PREINITIALIZED_CONTEXT -s USE_FREETYPE=1 --bind")
endif()
