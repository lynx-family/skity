// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include "src/render/hw/coverage/coverage_aa_line_encoder.hpp"

#include <gtest/gtest.h>

#include <cstddef>
#include <cstdint>
#include <vector>

namespace {

skity::CoverageAATileLine TileLine(uint32_t line_range_id, uint16_t from_x,
                                   uint16_t from_y, uint16_t to_x,
                                   uint16_t to_y) {
  return {from_x, from_y, to_x, to_y, line_range_id};
}

std::vector<uint32_t> LineRangeCounts(
    uint32_t line_range_count,
    const std::vector<skity::CoverageAATileLine>& lines) {
  std::vector<uint32_t> line_range_counts(line_range_count);
  for (const auto& line : lines) {
    line_range_counts[line.line_range_id]++;
  }
  return line_range_counts;
}

uint16_t ReadFromX(const skity::CoverageAAEncodedLines& encoded,
                   uint32_t line_index) {
  return encoded.texture_data[static_cast<size_t>(line_index) * 4];
}

}  // namespace

TEST(CoverageAALineEncoder, EncodesLineAsRGBA16UintTexel) {
  std::vector<skity::CoverageAATileLine> lines{
      TileLine(0, 0x1234, 0x5678, 0x9ABC, 0xDEF0)};
  auto line_range_counts = LineRangeCounts(1, lines);

  skity::CoverageAAEncodedLines encoded;
  skity::EncodeCoverageAALines(lines, line_range_counts, 16, encoded);

  EXPECT_EQ(encoded.texture_width, 16u);
  EXPECT_EQ(encoded.texture_height, 1u);
  EXPECT_EQ(encoded.texture_data.size(), 64u);
  EXPECT_EQ((std::vector<uint16_t>{encoded.texture_data.begin(),
                                   encoded.texture_data.begin() + 4}),
            (std::vector<uint16_t>{0x1234, 0x5678, 0x9ABC, 0xDEF0}));
  EXPECT_TRUE(line_range_counts.empty());
  EXPECT_EQ(encoded.range_offsets, (std::vector<uint32_t>{0, 1}));
}

TEST(CoverageAALineEncoder, GroupsLinesStablyByLineRange) {
  std::vector<skity::CoverageAATileLine> lines{
      TileLine(1, 0x0101, 0, 0, 1), TileLine(0, 0x0202, 0, 0, 1),
      TileLine(1, 0x0303, 0, 0, 1), TileLine(2, 0x0404, 0, 0, 1)};
  auto line_range_counts = LineRangeCounts(3, lines);

  skity::CoverageAAEncodedLines encoded;
  skity::EncodeCoverageAALines(lines, line_range_counts, 4, encoded);

  EXPECT_TRUE(line_range_counts.empty());
  EXPECT_EQ(encoded.range_offsets, (std::vector<uint32_t>{0, 1, 3, 4}));

  EXPECT_EQ(ReadFromX(encoded, 0), 0x0202);
  EXPECT_EQ(ReadFromX(encoded, 1), 0x0101);
  EXPECT_EQ(ReadFromX(encoded, 2), 0x0303);
  EXPECT_EQ(ReadFromX(encoded, 3), 0x0404);
}

TEST(CoverageAALineEncoder, EncodesAcrossTextureRows) {
  std::vector<skity::CoverageAATileLine> lines{
      TileLine(0, 1, 0, 0, 1), TileLine(0, 2, 0, 0, 1), TileLine(0, 3, 0, 0, 1),
      TileLine(0, 4, 0, 0, 1), TileLine(0, 5, 0, 0, 1)};
  auto line_range_counts = LineRangeCounts(1, lines);

  skity::CoverageAAEncodedLines encoded;
  skity::EncodeCoverageAALines(lines, line_range_counts, 4, encoded);

  EXPECT_EQ(encoded.texture_width, 4u);
  EXPECT_EQ(encoded.texture_height, 2u);
  EXPECT_EQ(encoded.texture_data.size(), 32u);
  EXPECT_EQ(ReadFromX(encoded, 0), 1u);
  EXPECT_EQ(ReadFromX(encoded, 1), 2u);
  EXPECT_EQ(ReadFromX(encoded, 2), 3u);
  EXPECT_EQ(ReadFromX(encoded, 3), 4u);
  EXPECT_EQ(ReadFromX(encoded, 4), 5u);
}

TEST(CoverageAALineEncoder, EmitsBindableTextureForEmptyInput) {
  std::vector<skity::CoverageAATileLine> lines;
  auto line_range_counts = LineRangeCounts(2, lines);

  skity::CoverageAAEncodedLines encoded;
  skity::EncodeCoverageAALines(lines, line_range_counts, 16, encoded);

  EXPECT_EQ(encoded.texture_width, 16u);
  EXPECT_EQ(encoded.texture_height, 1u);
  EXPECT_EQ(encoded.texture_data, std::vector<uint16_t>(64, 0));
  EXPECT_TRUE(line_range_counts.empty());
  EXPECT_EQ(encoded.range_offsets, (std::vector<uint32_t>{0, 0, 0}));
}

TEST(CoverageAALineEncoder, UsesPowerOfTwoWidthCappedAt4096) {
  std::vector<skity::CoverageAATileLine> lines;
  for (uint32_t i = 0; i < 17; ++i) {
    lines.push_back(TileLine(0, static_cast<uint16_t>(i), 0, 0, 1));
  }
  auto line_range_counts = LineRangeCounts(1, lines);

  skity::CoverageAAEncodedLines encoded;
  skity::EncodeCoverageAALines(lines, line_range_counts, 8192, encoded);
  EXPECT_EQ(encoded.texture_width, 32u);
  EXPECT_EQ(encoded.texture_height, 1u);

  for (uint32_t i = 17; i < 4097; ++i) {
    lines.push_back(TileLine(0, static_cast<uint16_t>(i), 0, 0, 1));
  }
  line_range_counts = LineRangeCounts(1, lines);
  skity::EncodeCoverageAALines(lines, line_range_counts, 8192, encoded);
  EXPECT_EQ(encoded.texture_width, 4096u);
  EXPECT_EQ(encoded.texture_height, 2u);
}
