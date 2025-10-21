// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include <skity/text/text_blob.hpp>

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
  buffer.WriteRect(blob.GetBoundsRect());

  const auto& runs = blob.GetTextRun();

  for (const auto& run : runs) {
    const auto& glyphs = run.GetGlyphInfo();

    if (glyphs.empty()) {
      continue;
    }

    buffer.WriteInt32(static_cast<int32_t>(glyphs.size()));

    int32_t run_position = 0;
    if (!run.GetPosX().empty()) {
      run_position = 1;

      if (!run.GetPosY().empty()) {
        run_position = 2;
      }
    }

    std::vector<float> pos_data{};

    if (run_position > 0) {
      const float* pos_x = run.GetPosX().data();
      const float* pos_y = run_position == 2 ? run.GetPosY().data() : nullptr;

      for (size_t i = 0; i < glyphs.size(); i++) {
        pos_data.push_back(pos_x[i]);
        if (run_position == 2) {
          pos_data.push_back(pos_y[i]);
        }
      }
    }

    buffer.WriteInt32(run_position);
    // no extended data

    buffer.WritePoint({0.f, 0.f});  // run offset, always (0, 0)

    const auto& font = run.GetFont();

    FlatIntoMemory(font, buffer);  // write font info first

    // write glyph info
    buffer.WriteByteArray(reinterpret_cast<const uint8_t*>(glyphs.data()),
                          sizeof(GlyphID) * glyphs.size());
    // write pos data
    buffer.WriteByteArray(reinterpret_cast<const uint8_t*>(pos_data.data()),
                          sizeof(float) * pos_data.size());
  }

  // Mark end of text blob
  buffer.WriteInt32(0);
}

template <>
std::shared_ptr<TextBlob> ReadFromMemory(ReadBuffer& buffer) {
  auto bounds = buffer.ReadRect();

  if (!buffer.Validate(bounds.has_value())) {
    return {};
  }

  std::vector<TextRun> runs{};

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

    std::vector<float> pos_x;
    std::vector<float> pos_y;

    if (pos == kHorizontal_Positioning) {
      pos_x = std::vector<float>(pos_data.begin(), pos_data.end());
    } else if (pos != kDefault_Positioning) {
      auto pos_ptr = pos_data.data();
      auto data_width = static_cast<uint32_t>(pos);
      for (int32_t i = 0; i < glyph_count; i++) {
        pos_x.push_back(pos_ptr[0]);
        pos_y.push_back(pos_ptr[1]);

        pos_ptr += data_width;
      }
    }

    runs.emplace_back(font.value(), std::move(glyphs_data), std::move(pos_x),
                      std::move(pos_y));
  }

  if (runs.empty()) {
    return {};
  }

  return std::make_shared<TextBlob>(std::move(runs));
}

}  // namespace skity
