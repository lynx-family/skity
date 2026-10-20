// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include <skity/text/text_blob.hpp>

#include "src/io/core_bridge.hpp"
#include "src/io/memory_read.hpp"
#include "src/io/memory_writer.hpp"

namespace skity {

namespace {
union PositioningAndExtended {
  int32_t intValue;
  struct {
    uint8_t positioning;
    uint8_t extended;
    uint16_t padding;
  };
};

enum GlyphPositioning : uint8_t {
  kDefault_Positioning =
      0,  // Default glyph advances -- zero scalars per glyph.
  kHorizontal_Positioning =
      1,                     // Horizontal positioning -- one scalar per glyph.
  kFull_Positioning = 2,     // Point positioning -- two scalars per glyph.
  kRSXform_Positioning = 3,  // RSXform positioning -- four scalars per glyph.
};

uint32_t scalars_per_glyph(GlyphPositioning pos) {
  constexpr uint8_t kScalarsPerGlyph[] = {
      0,  // kDefault_Positioning
      1,  // kHorizontal_Positioning
      2,  // kFull_Positioning
      4,  // kRSXform_Positioning
  };

  return kScalarsPerGlyph[pos];
}

}  // namespace

template <>
void FlatIntoMemory(const TextBlob& blob, WriteBuffer& buffer) {
  buffer.WriteRect(core_bridge::GetTextBlobBounds(&blob));

  core_bridge::WithTextBlobHandle(&blob, [&](skity_text_blob handle) {
    int32_t run_count =
        handle != nullptr ? skity_text_blob_get_run_count(handle) : 0;

    for (int32_t run_index = 0; run_index < run_count; run_index++) {
      int32_t glyph_count =
          skity_text_blob_get_run_glyph_count(handle, run_index);

      if (glyph_count <= 0) {
        continue;
      }

      std::vector<uint16_t> glyphs(glyph_count);
      skity_text_blob_get_run_glyphs(handle, run_index, glyphs.data(),
                                     glyph_count);

      int32_t pos_x_count =
          skity_text_blob_get_run_pos_x(handle, run_index, nullptr, 0);
      int32_t pos_y_count =
          skity_text_blob_get_run_pos_y(handle, run_index, nullptr, 0);
      std::vector<float> pos_x(pos_x_count > 0 ? pos_x_count : 0);
      std::vector<float> pos_y(pos_y_count > 0 ? pos_y_count : 0);
      if (!pos_x.empty()) {
        skity_text_blob_get_run_pos_x(handle, run_index, pos_x.data(),
                                      static_cast<int32_t>(pos_x.size()));
      }
      if (!pos_y.empty()) {
        skity_text_blob_get_run_pos_y(handle, run_index, pos_y.data(),
                                      static_cast<int32_t>(pos_y.size()));
      }

      buffer.WriteInt32(glyph_count);

      int32_t run_position = 0;
      if (!pos_x.empty()) {
        run_position = 1;

        if (!pos_y.empty()) {
          run_position = 2;
        }
      }

      std::vector<float> pos_data{};

      if (run_position > 0) {
        const float* pos_x_ptr = pos_x.data();
        const float* pos_y_ptr = run_position == 2 ? pos_y.data() : nullptr;

        for (size_t i = 0; i < glyphs.size(); i++) {
          pos_data.push_back(pos_x_ptr[i]);
          if (run_position == 2) {
            pos_data.push_back(pos_y_ptr[i]);
          }
        }
      }

      buffer.WriteInt32(run_position);
      // no extended data

      buffer.WritePoint({0.f, 0.f});  // run offset, always (0, 0)

      skity_font font_handle = skity_text_blob_get_run_font(handle, run_index);
      std::shared_ptr<Font> font = skity::capi::get_impl<skity_font_s, Font>(
          font_handle, SKITY_OBJECT_TYPE_FONT);
      skity_font_destroy(font_handle);

      FlatIntoMemory(*font, buffer);  // write font info first

      // write glyph info
      buffer.WriteByteArray(reinterpret_cast<const uint8_t*>(glyphs.data()),
                            sizeof(GlyphID) * glyphs.size());
      // write pos data
      buffer.WriteByteArray(reinterpret_cast<const uint8_t*>(pos_data.data()),
                            sizeof(float) * pos_data.size());
    }

    return 0;
  });

  // Mark end of text blob
  buffer.WriteInt32(0);
}

template <>
std::shared_ptr<TextBlob> ReadFromMemory(ReadBuffer& buffer) {
  auto bounds = buffer.ReadRect();

  if (!buffer.Validate(bounds.has_value())) {
    return {};
  }

  std::vector<core_bridge::TextRunData> runs{};

  for (;;) {
    auto glyph_count = buffer.ReadInt();

    if (glyph_count == 0) {
      // end of runs
      break;
    }

    PositioningAndExtended pe;
    pe.intValue = buffer.ReadInt();
    auto pos = static_cast<GlyphPositioning>(pe.positioning);

    if (glyph_count <= 0 || pos > kRSXform_Positioning) {
      return {};
    }

    auto text_size = pe.extended ? buffer.ReadInt() : 0;

    if (text_size < 0) {
      return {};
    }

    auto offset = buffer.ReadPoint();

    auto font = ReadFromMemory<std::optional<Font>>(buffer);

    if (!font.has_value()) {
      return {};
    }

    auto glyph_size = glyph_count * sizeof(GlyphID);
    auto pos_size = glyph_count * sizeof(float) * scalars_per_glyph(pos);
    auto cluster_size = pe.extended ? glyph_count * sizeof(uint32_t) : 0;

    auto total_size = glyph_size + pos_size + cluster_size + text_size;

    if (!buffer.Validate(total_size <= buffer.Available())) {
      return {};
    }

    std::vector<GlyphID> glyphs_data(glyph_count);
    std::vector<float> pos_data(pos_size / sizeof(float));

    if (!buffer.ReadArrayN<uint8_t>(glyphs_data.data(), glyph_size)) {
      return {};
    }

    if (!buffer.ReadArrayN<uint8_t>(pos_data.data(), pos_size)) {
      return {};
    }

    if (pe.extended) {
      // we do not support clusters and other data, just skip it
      buffer.Skip(cluster_size);
      buffer.Skip(text_size);
    }

    core_bridge::TextRunData run;
    run.font = std::make_shared<Font>(font.value());

    if (pos == kHorizontal_Positioning) {
      run.pos_x = std::vector<float>(pos_data.begin(), pos_data.end());
    } else if (pos != kDefault_Positioning) {
      auto pos_ptr = pos_data.data();
      auto data_width = static_cast<uint32_t>(pos);
      for (int32_t i = 0; i < glyph_count; i++) {
        run.pos_x.push_back(pos_ptr[0]);
        run.pos_y.push_back(pos_ptr[1]);

        pos_ptr += data_width;
      }
    }

    run.glyphs = std::move(glyphs_data);
    runs.emplace_back(std::move(run));
  }

  if (runs.empty()) {
    return {};
  }

  return core_bridge::MakeTextBlobFromRuns(runs);
}

}  // namespace skity
