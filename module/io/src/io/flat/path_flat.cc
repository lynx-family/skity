// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include <skity/c/skity_path.h>

#include <array>
#include <skity/graphic/path.hpp>

#include "src/io/core_bridge.hpp"
#include "src/io/memory_read.hpp"
#include "src/io/memory_writer.hpp"

namespace skity {

namespace {

enum SerializationOffsets {
  kType_SerializationShift = 28,       // requires 4 bits
  kDirection_SerializationShift = 26,  // requires 2 bits
  kFillType_SerializationShift = 8,    // requires 8 bits
  // low-8-bits are version
  kVersion_SerializationMask = 0xFF,
};

enum SerializationVersions {
  // kPathPrivFirstDirection_Version = 1,
  // kPathPrivLastMoveToIndex_Version = 2,
  // kPathPrivTypeEnumVersion = 3,
  kJustPublicData_Version = 4,         // introduced Feb/2018
  kVerbsAreStoredForward_Version = 5,  // introduced Sept/2019

  kMin_Version = kJustPublicData_Version,
  kCurrent_Version = kVerbsAreStoredForward_Version
};

enum SerializationType { kGeneral = 0, kRRect = 1 };

struct PathData {
  std::vector<Vec2> points;
  std::vector<float> weights;
  std::vector<uint8_t> verbs;
};

PathData QueryPathData(const Path& path) {
  PathData data;

  // Flat bulk read of the path storage (verbs + points + conic weights) and
  // a replay projecting each verb to the control points it contributes —
  // the same point sequence Path::Iter yields, minus each segment's start.
  std::shared_ptr<Path> view(const_cast<Path*>(&path), [](Path*) {});
  skity_path handle = skity::capi::alloc_handle<skity_path_s>(
      SKITY_OBJECT_TYPE_PATH, 0, std::move(view));

  if (handle != nullptr) {
    std::vector<skity_path_verb> verbs(
        skity_path_get_verbs(handle, nullptr, 0));
    std::vector<skity_point> points(skity_path_get_points(handle, nullptr, 0));
    std::vector<float> weights(
        skity_path_get_conic_weights(handle, nullptr, 0));
    skity_path_get_verbs(handle, verbs.data(),
                         static_cast<int32_t>(verbs.size()));
    skity_path_get_points(handle, points.data(),
                          static_cast<int32_t>(points.size()));
    skity_path_get_conic_weights(handle, weights.data(),
                                 static_cast<int32_t>(weights.size()));
    skity_path_destroy(handle);

    size_t pt = 0;
    size_t w = 0;
    for (skity_path_verb verb : verbs) {
      switch (verb) {
        // The point storage is incremental: every verb stores only the
        // points it introduces (move/line 1, quad/conic 2, cubic 3), so the
        // projected control points sit at [pt..] directly.
        case SKITY_PATH_VERB_MOVE:
          data.verbs.push_back(static_cast<uint8_t>(verb));
          data.points.push_back({points[pt].x, points[pt].y});
          pt += 1;
          break;
        case SKITY_PATH_VERB_LINE:
          data.verbs.push_back(static_cast<uint8_t>(verb));
          data.points.push_back({points[pt].x, points[pt].y});
          pt += 1;
          break;
        case SKITY_PATH_VERB_QUAD:
          data.verbs.push_back(static_cast<uint8_t>(verb));
          data.points.push_back({points[pt].x, points[pt].y});
          data.points.push_back({points[pt + 1].x, points[pt + 1].y});
          pt += 2;
          break;
        case SKITY_PATH_VERB_CUBIC:
          data.verbs.push_back(static_cast<uint8_t>(verb));
          data.points.push_back({points[pt].x, points[pt].y});
          data.points.push_back({points[pt + 1].x, points[pt + 1].y});
          data.points.push_back({points[pt + 2].x, points[pt + 2].y});
          pt += 3;
          break;
        case SKITY_PATH_VERB_CONIC:
          data.verbs.push_back(static_cast<uint8_t>(verb));
          data.points.push_back({points[pt].x, points[pt].y});
          data.points.push_back({points[pt + 1].x, points[pt + 1].y});
          data.weights.push_back(weights[w++]);
          pt += 2;
          break;
        case SKITY_PATH_VERB_CLOSE:
          data.verbs.push_back(static_cast<uint8_t>(verb));
          break;
        default:
          break;
      }
    }
  }

  return data;
}

uint32_t extract_version(uint32_t packed) {
  return packed & kVersion_SerializationMask;
}

SerializationType extract_serialization_type(uint32_t packed) {
  return static_cast<SerializationType>((packed >> kType_SerializationShift) &
                                        0xF);
}

Path::PathFillType extract_path_fill_type(uint32_t packed) {
  return static_cast<Path::PathFillType>(
      (packed >> kFillType_SerializationShift) & 0x3);
}

std::optional<Path> ReadRRectPath(ReadBuffer& buffer, uint32_t packed) {
  auto dir = (packed >> kDirection_SerializationShift) & 0x3;
  auto fill_type = extract_path_fill_type(packed);

  Path::Direction rrect_dir;
  IORRect rrect;
  int32_t start;

  switch (dir) {
    case static_cast<int32_t>(Path::Direction::kCW):
      rrect_dir = Path::Direction::kCW;
      break;
    case static_cast<int32_t>(Path::Direction::kCCW):
      rrect_dir = Path::Direction::kCCW;
      break;
    default:
      buffer.Validate(false);
      return std::nullopt;
  }

  {
    auto opt = buffer.ReadRRect();

    if (!opt) {
      buffer.Validate(false);
      return std::nullopt;
    }
    rrect = *opt;
  }

  start = buffer.ReadInt();
  if (start < 0 || start > 7) {
    buffer.Validate(false);
    return std::nullopt;
  }

  core_bridge::PathBuilder builder;

  builder.AddRRectStart(rrect.rect, rrect.radii, rrect_dir, start);
  builder.SetFillType(fill_type);

  buffer.SkipToAlign4();

  auto path = builder.Take();
  return path ? std::optional<Path>{*path} : std::nullopt;
}

void fill_path(core_bridge::PathBuilder& path, const Vec2* pts,
               const float* weights, const uint8_t* verbs, size_t verbs_count) {
  for (size_t i = 0; i < verbs_count; i++) {
    auto verb = static_cast<Path::Verb>(verbs[i]);
    switch (verb) {
      case Path::Verb::kMove:
        path.MoveTo(pts[0].x, pts[0].y);
        pts++;
        break;
      case Path::Verb::kLine:
        path.LineTo(pts[0].x, pts[0].y);
        pts++;
        break;
      case Path::Verb::kQuad:
        path.QuadTo(pts[0].x, pts[0].y, pts[1].x, pts[1].y);
        pts += 2;
        break;
      case Path::Verb::kCubic:
        path.CubicTo(pts[0].x, pts[0].y, pts[1].x, pts[1].y, pts[2].x,
                     pts[2].y);
        pts += 3;
        break;
      case Path::Verb::kConic:
        path.ConicTo(pts[0].x, pts[0].y, pts[1].x, pts[1].y, weights[0]);
        pts += 2;
        weights++;
        break;
      case Path::Verb::kClose:
        path.Close();
        break;
      case Path::Verb::kDone:
        break;
    }
  }
}

}  // namespace

template <>
void FlatIntoMemory(const Path& path, MemoryWriter32& writer) {
  auto path_data = QueryPathData(path);

  int32_t packed = (static_cast<int32_t>(path.GetFillType())
                    << kFillType_SerializationShift) |
                   (SerializationType::kGeneral << kType_SerializationShift) |
                   kCurrent_Version;

  int32_t pts = path_data.points.size();
  int32_t cnx = path_data.weights.size();
  int32_t vbs = path_data.verbs.size();

  size_t size = 4 * sizeof(int32_t);

  size += pts * sizeof(Vec2);
  size += cnx * sizeof(float);
  size += vbs * sizeof(uint8_t);

  size = Align4(size);

  if (size <= 0) {
    return;
  }

  auto ptr = writer.Reserve(size);

  SegmentBufferWriter raw_writer(reinterpret_cast<uint8_t*>(ptr), size);

  raw_writer.Write32(packed);
  raw_writer.Write32(pts);
  raw_writer.Write32(cnx);
  raw_writer.Write32(vbs);

  raw_writer.Write(path_data.points.data(), pts * sizeof(Vec2));
  raw_writer.Write(path_data.weights.data(), cnx * sizeof(float));
  raw_writer.Write(path_data.verbs.data(), vbs * sizeof(uint8_t));
  raw_writer.PadToAlign4();
}

template <>
std::optional<Path> ReadFromMemory(ReadBuffer& buffer) {
  uint32_t packed = buffer.ReadU32();

  if (!buffer.IsValid()) {
    return std::nullopt;
  }

  auto version = extract_version(packed);
  bool verbs_are_stored_forward = version == kVerbsAreStoredForward_Version;

  if (!verbs_are_stored_forward && version != kJustPublicData_Version) {
    // unsupport version
    buffer.Validate(false);

    return std::nullopt;
  }

  // handle serializtion type
  switch (extract_serialization_type(packed)) {
    case SerializationType::kRRect:
      return ReadRRectPath(buffer, packed);
    case SerializationType::kGeneral:
      // default general path
      break;
    default:
      // unknown type
      buffer.Validate(false);
      return std::nullopt;
  }

  std::array<int32_t, 3> path_infos{};  // pts, cnx, vbs
  if (!buffer.ReadPad32(path_infos.data(), sizeof(path_infos))) {
    return std::nullopt;
  }

  std::vector<Vec2> points(path_infos[0]);
  std::vector<float> weights(path_infos[1]);
  std::vector<uint8_t> verbs(path_infos[2]);

  buffer.SkipToAlign4();

  if (!buffer.IsValid()) {
    return std::nullopt;
  }

  if (verbs.empty()) {
    if (points.empty() && weights.empty()) {
      return {Path()};
    }

    buffer.Validate(false);
    return std::nullopt;
  }

  if (!buffer.ReadPad32(points.data(), points.size() * sizeof(Vec2))) {
    return std::nullopt;
  }

  if (!buffer.ReadPad32(weights.data(), weights.size() * sizeof(float))) {
    return std::nullopt;
  }

  if (!buffer.ReadPad32(verbs.data(), verbs.size() * sizeof(uint8_t))) {
    return std::nullopt;
  }

  buffer.SkipToAlign4();
  if (!buffer.IsValid()) {
    return std::nullopt;
  }

  core_bridge::PathBuilder builder;

  fill_path(builder, points.data(), weights.data(), verbs.data(), verbs.size());

  builder.SetFillType(extract_path_fill_type(packed));

  auto path = builder.Take();
  return path ? std::optional<Path>{*path} : std::nullopt;
}

}  // namespace skity
