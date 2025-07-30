// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include "src/render/text/atlas/atlas_manager.hpp"

#include <skity/text/font.hpp>

#include "src/geometry/math.hpp"
#include "src/gpu/gpu_context_impl.hpp"
#include "src/render/text/atlas/atlas_texture.hpp"
#include "src/render/text/sdf_gen.hpp"
#include "src/render/text/text_render_control.hpp"
#include "src/tracing.hpp"

namespace skity {

namespace {

bool valid_positive_float(float f) {
  return !(FloatIsNan(f) || !FloatIsFinite(f) || f < 0.f);
}

}  // namespace

/// AtlasManager
AtlasManager::AtlasManager(GPUDevice* gpu_device, GPUContextImpl* gpu_context)
    : gpu_device_(gpu_device), gpu_context_(gpu_context) {}

Atlas* AtlasManager::GetAtlas(AtlasFormat format) {
  size_t index = static_cast<size_t>(format);
  if (!atlas_[index]) {
    bool enable_larger_atlas =
        gpu_context_->GetLargerAtlasMask() & (1 << index);
    atlas_[index] =
        std::make_unique<Atlas>(format, gpu_device_, enable_larger_atlas);
  }
  return atlas_[index].get();
}

void AtlasManager::ClearExtraRes() {
  for (size_t i = 0; i < 2; i++) {
    if (atlas_[i]) {
      atlas_[i]->ClearExtraRes();
    }
  }
}

/// Atlas
Atlas::Atlas(AtlasFormat format, GPUDevice* gpu_device,
             bool enable_larger_atlas)
    : format_(format),
      gpu_device_(gpu_device),
      atlas_config_(format, enable_larger_atlas) {
  switch (format_) {
    case AtlasFormat::A8:
      bytes_per_pixel_ = 1;
      break;
    case AtlasFormat::RGBA32:
      bytes_per_pixel_ = 4;
      break;
  }
}

GlyphRegion Atlas::GetGlyphRegion(const Font& font, GlyphID glyph_id,
                                  const Paint& paint, bool load_sdf,
                                  float context_scale,
                                  const Matrix& transform) {
  Typeface* typeface = font.GetTypeface();
  float font_size = font.GetSize();
  float sdf_scale = 1.0f;
  // TODO(jingle) consider transform for sdf text
  float text_size = font_size * context_scale;
  if (load_sdf) {
    if (text_size <= kSmallDFFontSize) {
      sdf_scale = text_size / kSmallDFFontSize;
      text_size = kSmallDFFontSize;
    } else if (text_size <= kMediumDFFontSize) {
      sdf_scale = text_size / kMediumDFFontSize;
      text_size = kMediumDFFontSize;
    } else {
      sdf_scale = text_size / kLargeDFFontSize;
      text_size = kLargeDFFontSize;
    }
  }

  ScalerContextDesc scaler_context_desc;
  scaler_context_desc.typeface_id = typeface->TypefaceId();
  scaler_context_desc.text_size = load_sdf ? text_size : font_size;
  scaler_context_desc.scale_x = font.GetScaleX();
  scaler_context_desc.skew_x = font.GetSkewX();
  if (load_sdf) {
    Matrix22 transform22{1.f, 0.f, 0.f, 1.f};
    scaler_context_desc.transform = transform22;
  } else {
    Matrix22 transform22{transform.GetScaleX(), transform.GetSkewX(),
                         transform.GetSkewY(), transform.GetScaleY()};
    scaler_context_desc.transform = transform22;
  }
  scaler_context_desc.context_scale = load_sdf ? 1.f : context_scale;

  scaler_context_desc.stroke_width =
      paint.GetStyle() == Paint::kStroke_Style ? paint.GetStrokeWidth() : 0.f;
  scaler_context_desc.miter_limit = paint.GetStyle() == Paint::kStroke_Style
                                        ? paint.GetStrokeMiter()
                                        : Paint::kDefaultMiterLimit;
  scaler_context_desc.cap = paint.GetStyle() == Paint::kStroke_Style
                                ? paint.GetStrokeCap()
                                : Paint::kDefault_Cap;
  scaler_context_desc.join = paint.GetStyle() == Paint::kStroke_Style
                                 ? paint.GetStrokeJoin()
                                 : Paint::kDefault_Join;
  scaler_context_desc.fake_bold = font.IsEmbolden() ? 1 : 0;
  GlyphKey key(glyph_id, scaler_context_desc);

  for (uint32_t index = 0; index < atlas_bitmap_.size(); index++) {
    if (atlas_bitmap_[index]) {
      AtlasBitmap* memory_atlas = atlas_bitmap_[index].get();
      auto region = memory_atlas->GetGlyphRegion(key);
      if (region != INVALID_LOC) {
        return GlyphRegion{index, region, sdf_scale};
      }
    }
  }

  GlyphRegion gen_region = GenerateGlyphRegion(font, key, paint, load_sdf);
  return GlyphRegion{gen_region.index_in_group, gen_region.loc, sdf_scale};
}

GlyphRegion Atlas::GenerateGlyphRegion(const Font& font, GlyphKey const& key,
                                       const Paint& paint, bool load_sdf) {
  SKITY_TRACE_EVENT(Atlas_GenerateGlyphRegion);
  //  generate text bitmap from typeface
  Font resized_font(font);
  resized_font.SetSize(key.scaler_context_desc.text_size);
  const GlyphData* glyph_data;

  Paint fill_paint = paint;
  if (load_sdf) {
    // sdf generation algorithm utilize fill styled glyph as source
    fill_paint.SetStyle(Paint::kFill_Style);
  }

  resized_font.LoadGlyphBitmap(&(key.glyph_id), 1, &glyph_data, fill_paint,
                               key.scaler_context_desc.context_scale,
                               key.scaler_context_desc.transform.ToMatrix());
  const auto& bitmap_info = glyph_data->Image();

  if (fill_paint.GetStyle() == Paint::kStroke_Style &&
      fill_paint.IsAdjustStroke()) {
    const GlyphData* glyph_data_fill;
    fill_paint.SetStyle(Paint::kFill_Style);
    resized_font.LoadGlyphBitmap(&(key.glyph_id), 1, &glyph_data_fill,
                                 fill_paint,
                                 key.scaler_context_desc.context_scale,
                                 key.scaler_context_desc.transform.ToMatrix());
    fill_paint.SetStyle(Paint::kStroke_Style);

    if (valid_positive_float(bitmap_info.width) &&
        valid_positive_float(bitmap_info.height) &&
        valid_positive_float(glyph_data_fill->Image().width) &&
        valid_positive_float(glyph_data_fill->Image().height)) {
      size_t width = static_cast<size_t>(bitmap_info.width);
      size_t height = static_cast<size_t>(bitmap_info.height);
      size_t width_fill = static_cast<size_t>(glyph_data_fill->Image().width);
      size_t height_fill = static_cast<size_t>(glyph_data_fill->Image().height);

      // Presume size of stroked text be >= filled text
      if (width >= width_fill && height >= height_fill) {
        size_t width_offset = (width - width_fill) / 2;
        size_t height_offset = (height - height_fill) / 2;

        auto bitmap_fill_buffer = glyph_data_fill->Image().buffer;
        auto bitmap_stroke_buffer = bitmap_info.buffer;
        size_t initial_index_stroke = height_offset * width;

        for (size_t h = 0; h < height_fill; ++h) {
          size_t index_stroke =
              initial_index_stroke +
              width_offset;  // Compute the initial stroke index
          size_t index_fill = h * width_fill;  // Start of the fill row
          for (size_t w = 0; w < width_fill;
               ++w, ++index_fill, ++index_stroke) {
            if (bitmap_fill_buffer[index_fill] == 0xff) {
              bitmap_stroke_buffer[index_stroke] = 0;
            }
          }
          initial_index_stroke +=
              width;  // Move the stroke index to the next row
        }
      }
    }
    if (glyph_data_fill->NeedFree()) {
      std::free(glyph_data_fill->Image().buffer);
      const_cast<GlyphBitmapData&>(glyph_data_fill->Image()).need_free = false;
    }
  }

  if (load_sdf) {
    size_t width = static_cast<size_t>(bitmap_info.width);
    size_t height = static_cast<size_t>(bitmap_info.height);
    sdf::Image<uint8_t> unfiltered_image(width, height);
    size_t unfiltered_image_size = width * height;
    for (size_t index = 0; index < unfiltered_image_size; index++) {
      size_t x = index % width;
      size_t y = index / width;
      unfiltered_image.Set(x, y, bitmap_info.buffer[index]);
    }

    sdf::Image<uint8_t> filtered_image =
        sdf::SdfGen::GenerateSdfImage(unfiltered_image);
    const_cast<GlyphBitmapData&>(bitmap_info).width = filtered_image.GetWidth();
    const_cast<GlyphBitmapData&>(bitmap_info).height =
        filtered_image.GetHeight();
    if (bitmap_info.need_free) {
      std::free(bitmap_info.buffer);
      const_cast<GlyphBitmapData&>(bitmap_info).need_free = false;
    }
    const_cast<GlyphBitmapData&>(bitmap_info).buffer =
        filtered_image.GetRawData();

    return GenerateGlyphRegionInternal(key, bitmap_info);
  }

  return GenerateGlyphRegionInternal(key, bitmap_info);
}

GlyphRegion Atlas::GenerateGlyphRegionInternal(
    const GlyphKey& key, const GlyphBitmapData& glyph_bitmap) {
  SKITY_TRACE_EVENT(Atlas_GenerateGlyphRegionInternal);
  if (current_bitmap_index_ >= atlas_bitmap_.size()) {
    atlas_bitmap_.resize(current_bitmap_index_ + 1);
  }
  if (!atlas_bitmap_[current_bitmap_index_]) {
    atlas_bitmap_[current_bitmap_index_] = std::make_unique<AtlasBitmap>(
        atlas_config_.max_bitmap_size, atlas_config_.max_bitmap_size,
        bytes_per_pixel_);
  }
  AtlasBitmap* atlas_bitmap = atlas_bitmap_[current_bitmap_index_].get();
  auto region = atlas_bitmap->GenerateGlyphRegion(key, glyph_bitmap);
  if (region != INVALID_LOC) {
    if (glyph_bitmap.need_free) {
      std::free(glyph_bitmap.buffer);
      const_cast<GlyphBitmapData&>(glyph_bitmap).need_free = false;
    }
    return GlyphRegion{current_bitmap_index_, region, 1.f};
  } else {
    current_bitmap_index_ = atlas_bitmap_.size();
    return GenerateGlyphRegionInternal(key, glyph_bitmap);
  }
}

// upload atlas from memory storage to gpu texture
void Atlas::UploadAtlas(uint32_t group_index) {
  SKITY_TRACE_EVENT(Atlas_UploadAtlas);
  uint32_t start = group_index * atlas_config_.max_num_bitmap_per_atlas;
  uint32_t upload_count = atlas_bitmap_.size() - start;
  if (upload_count > atlas_config_.max_num_bitmap_per_atlas) {
    upload_count = atlas_config_.max_num_bitmap_per_atlas;
  }
  for (uint32_t index = start; index < start + upload_count; index++) {
    if (atlas_bitmap_[index]) {
      const std::optional<glm::ivec4> dirty_rect =
          atlas_bitmap_[index]->DirtyRect();
      if (dirty_rect.has_value()) {
        if (group_index >= atlas_texture_array_.size()) {
          atlas_texture_array_.resize(group_index + 1);
        }
        if (!atlas_texture_array_[group_index]) {
          atlas_texture_array_[group_index] =
              std::make_unique<AtlasTextureArray>(
                  atlas_config_.max_texture_size,
                  atlas_config_.max_texture_size, format_, gpu_device_);
        }
        uint8_t* mem_data = atlas_bitmap_[index]->MemData();

        uint32_t texture_index_in_atlas =
            ((index % atlas_config_.max_num_bitmap_per_atlas) /
             atlas_config_.max_num_bitmap_per_texture);
        uint32_t bitmap_index_in_texture =
            index % atlas_config_.max_num_bitmap_per_texture;

        uint32_t start_x = atlas_config_.max_bitmap_size *
                           (bitmap_index_in_texture & atlas_config_.col_mask);
        uint32_t start_y =
            atlas_config_.max_bitmap_size *
            ((bitmap_index_in_texture & atlas_config_.row_mask) >>
             atlas_config_.row_shift);
        atlas_texture_array_[group_index]->UploadAtlas(
            texture_index_in_atlas, start_x, start_y + dirty_rect->y,
            atlas_config_.max_bitmap_size, dirty_rect->w - dirty_rect->y,
            mem_data + atlas_config_.max_bitmap_size * dirty_rect->y *
                           bytes_per_pixel_);
        atlas_bitmap_[index]->SetAllClean();
      }
    }
  }
}

Vec2 Atlas::CalculateUV(uint32_t bitmap_index, uint32_t x, uint32_t y) {
  uint32_t texture_index_in_atlas =
      ((bitmap_index % atlas_config_.max_num_bitmap_per_atlas) /
       atlas_config_.max_num_bitmap_per_texture);
  uint32_t bitmap_index_in_texture =
      bitmap_index % atlas_config_.max_num_bitmap_per_texture;

  uint32_t start_x = atlas_config_.max_bitmap_size *
                     (bitmap_index_in_texture & atlas_config_.col_mask);
  uint32_t start_y = atlas_config_.max_bitmap_size *
                     ((bitmap_index_in_texture & atlas_config_.row_mask) >>
                      atlas_config_.row_shift);

  uint32_t u = start_x + x;

  u |= (texture_index_in_atlas & 0x3) << 14;
  uint32_t v = start_y + y;

  return {static_cast<float>(u), static_cast<float>(v)};
}

std::array<std::shared_ptr<GPUTexture>, AtlasConfig::MAX_NUM_TEXTURE_PER_ATLAS>
Atlas::GetGPUTexture(uint32_t index) {
  return atlas_texture_array_[index]->GetTextures();
}

std::array<std::shared_ptr<GPUSampler>, AtlasConfig::MAX_NUM_TEXTURE_PER_ATLAS>
Atlas::GetGPUSamplers(uint32_t index, GPUFilterMode filter_mode) {
  GPUSamplerDescriptor descriptor;
  descriptor.mag_filter = filter_mode;
  descriptor.min_filter = filter_mode;
  return atlas_texture_array_[index]->GetSamplers(descriptor);
}

std::shared_ptr<GPUSampler> Atlas::GetGPUSampler(uint32_t index,
                                                 GPUFilterMode filter_mode) {
  GPUSamplerDescriptor descriptor;
  descriptor.mag_filter = filter_mode;
  descriptor.min_filter = filter_mode;
  return atlas_texture_array_[index]->GetSampler(descriptor);
}

void Atlas::ClearExtraRes() {
  if (atlas_bitmap_.size() > atlas_config_.max_num_bitmap_per_atlas) {
    uint32_t start = atlas_config_.max_num_bitmap_per_atlas;
    uint32_t end = atlas_config_.max_num_bitmap_per_atlas * 2;
    if (end <= atlas_bitmap_.size()) {
      // > 2x
      for (uint32_t i = 0; i < atlas_config_.max_num_bitmap_per_atlas; i++) {
        std::swap(atlas_bitmap_[atlas_bitmap_.size() -
                                atlas_config_.max_num_bitmap_per_atlas + i],
                  atlas_bitmap_[i]);
        atlas_bitmap_[i]->SetAllDirty();
      }
      least_used_index_ = 0;
      current_bitmap_index_ = atlas_config_.max_num_bitmap_per_atlas - 1;
      atlas_bitmap_.resize(atlas_config_.max_num_bitmap_per_atlas);
    } else {
      //  1x ~ 2x
      end = atlas_bitmap_.size();
      for (uint32_t i = start; i < end; i++) {
        atlas_bitmap_[i]->SetAllDirty();
        std::swap(atlas_bitmap_[least_used_index_], atlas_bitmap_[i]);
        if (i == current_bitmap_index_) {
          current_bitmap_index_ = least_used_index_;
        }
        least_used_index_ =
            (least_used_index_ + 1) % atlas_config_.max_num_bitmap_per_atlas;
      }
      atlas_bitmap_.resize(atlas_config_.max_num_bitmap_per_atlas);
    }
  }
  if (atlas_texture_array_.size() > 1) {
    atlas_texture_array_.resize(1);
  }
}

}  // namespace skity
