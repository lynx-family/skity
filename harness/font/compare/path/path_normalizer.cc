// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include "harness/font/compare/path/path_normalizer.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <skity/geometry/point.hpp>
#include <skity/geometry/rect.hpp>
#include <string>
#include <vector>

namespace skity {
namespace font_harness {

namespace {

struct NormalizedPoint {
  double x = 0.0;
  double y = 0.0;
  bool finite = true;
};

struct ContourState {
  bool active = false;
  bool closed = false;
  bool has_bounds = false;
  Json::UInt64 index = 0;
  Json::UInt64 start_verb_index = 0;
  Json::UInt64 end_verb_index = 0;
  Json::UInt64 verb_count = 0;
  double left = 0.0;
  double top = 0.0;
  double right = 0.0;
  double bottom = 0.0;
};

double NormalizeScalar(double value, double epsilon) {
  if (!std::isfinite(value)) {
    return value;
  }
  if (epsilon > 0.0) {
    value = std::round(value / epsilon) * epsilon;
  }
  const double zero_epsilon = epsilon > 0.0 ? epsilon : kDefaultPathEpsilon;
  if (value == 0.0 || std::fabs(value) < zero_epsilon * 0.5) {
    return 0.0;
  }
  return value;
}

Json::Value ScalarToJson(double value, double epsilon) {
  if (!std::isfinite(value)) {
    return Json::Value(Json::nullValue);
  }
  return Json::Value(NormalizeScalar(value, epsilon));
}

std::string FillTypeToString(Path::PathFillType fill_type) {
  switch (fill_type) {
    case Path::PathFillType::kWinding:
      return "winding";
    case Path::PathFillType::kEvenOdd:
      return "even_odd";
  }
  return "unknown";
}

std::string VerbToString(Path::Verb verb) {
  switch (verb) {
    case Path::Verb::kMove:
      return "move";
    case Path::Verb::kLine:
      return "line";
    case Path::Verb::kQuad:
      return "quad";
    case Path::Verb::kConic:
      return "conic";
    case Path::Verb::kCubic:
      return "cubic";
    case Path::Verb::kClose:
      return "close";
    case Path::Verb::kDone:
      return "done";
  }
  return "unknown";
}

int SerializedPointCount(Path::Verb verb) {
  switch (verb) {
    case Path::Verb::kMove:
      return 1;
    case Path::Verb::kLine:
      return 2;
    case Path::Verb::kQuad:
    case Path::Verb::kConic:
      return 3;
    case Path::Verb::kCubic:
      return 4;
    case Path::Verb::kClose:
    case Path::Verb::kDone:
      return 0;
  }
  return 0;
}

bool IsSegmentVerb(Path::Verb verb) {
  return verb == Path::Verb::kLine || verb == Path::Verb::kQuad ||
         verb == Path::Verb::kConic || verb == Path::Verb::kCubic;
}

NormalizedPoint NormalizePoint(const Point& point, double epsilon) {
  NormalizedPoint value;
  value.finite = std::isfinite(point.x) && std::isfinite(point.y);
  value.x = NormalizeScalar(point.x, epsilon);
  value.y = NormalizeScalar(point.y, epsilon);
  return value;
}

Json::Value PointToJson(const NormalizedPoint& point, double epsilon) {
  Json::Value value(Json::objectValue);
  value["x"] = ScalarToJson(point.x, epsilon);
  value["y"] = ScalarToJson(point.y, epsilon);
  return value;
}

bool SamePoint(const NormalizedPoint& lhs, const NormalizedPoint& rhs) {
  return lhs.finite && rhs.finite && lhs.x == rhs.x && lhs.y == rhs.y;
}

bool IsZeroLengthSegment(Path::Verb verb,
                         const std::vector<NormalizedPoint>& points) {
  if (!IsSegmentVerb(verb) || points.empty()) {
    return false;
  }
  const NormalizedPoint& first = points.front();
  for (const auto& point : points) {
    if (!SamePoint(first, point)) {
      return false;
    }
  }
  return true;
}

Json::Value BoundsToJson(double left, double top, double right, double bottom,
                         bool has_bounds, double epsilon) {
  Json::Value value(Json::objectValue);
  value["left"] = ScalarToJson(has_bounds ? left : 0.0, epsilon);
  value["top"] = ScalarToJson(has_bounds ? top : 0.0, epsilon);
  value["right"] = ScalarToJson(has_bounds ? right : 0.0, epsilon);
  value["bottom"] = ScalarToJson(has_bounds ? bottom : 0.0, epsilon);
  value["width"] = ScalarToJson(has_bounds ? right - left : 0.0, epsilon);
  value["height"] = ScalarToJson(has_bounds ? bottom - top : 0.0, epsilon);
  value["empty"] = !has_bounds || !(left < right && top < bottom);
  return value;
}

Json::Value RectToJson(const Rect& rect, double epsilon) {
  return BoundsToJson(rect.Left(), rect.Top(), rect.Right(), rect.Bottom(),
                      rect.IsFinite(), epsilon);
}

void AddPointToContour(const NormalizedPoint& point, ContourState* contour) {
  if (!point.finite) {
    return;
  }
  if (!contour->has_bounds) {
    contour->left = point.x;
    contour->right = point.x;
    contour->top = point.y;
    contour->bottom = point.y;
    contour->has_bounds = true;
    return;
  }
  contour->left = std::min(contour->left, point.x);
  contour->right = std::max(contour->right, point.x);
  contour->top = std::min(contour->top, point.y);
  contour->bottom = std::max(contour->bottom, point.y);
}

Json::Value ContourToJson(const ContourState& contour, double epsilon) {
  Json::Value value(Json::objectValue);
  value["index"] = contour.index;
  value["start_verb_index"] = contour.start_verb_index;
  value["end_verb_index"] = contour.end_verb_index;
  value["verb_count"] = contour.verb_count;
  value["closed"] = contour.closed;
  value["bounds"] = BoundsToJson(contour.left, contour.top, contour.right,
                                 contour.bottom, contour.has_bounds, epsilon);
  return value;
}

void FinishContour(ContourState* contour, Json::Value* contours,
                   double epsilon) {
  if (!contour->active) {
    return;
  }
  contours->append(ContourToJson(*contour, epsilon));
  *contour = ContourState{};
}

void AddVerbToContour(Path::Verb verb,
                      const std::vector<NormalizedPoint>& points,
                      Json::UInt64 verb_index, Json::Value* contours,
                      ContourState* contour, double epsilon) {
  if (verb == Path::Verb::kMove) {
    FinishContour(contour, contours, epsilon);
    contour->active = true;
    contour->index = contours->size();
    contour->start_verb_index = verb_index;
  } else if (!contour->active && verb != Path::Verb::kClose) {
    contour->active = true;
    contour->index = contours->size();
    contour->start_verb_index = verb_index;
  }

  if (!contour->active) {
    return;
  }

  contour->end_verb_index = verb_index;
  contour->verb_count += 1;
  for (const auto& point : points) {
    AddPointToContour(point, contour);
  }

  if (verb == Path::Verb::kClose) {
    contour->closed = true;
    FinishContour(contour, contours, epsilon);
  }
}

Json::Value SegmentMasksToJson(uint32_t masks) {
  Json::Value value(Json::objectValue);
  value["line"] = (masks & Path::SegmentMask::kLine) != 0;
  value["quad"] = (masks & Path::SegmentMask::kQuad) != 0;
  value["conic"] = (masks & Path::SegmentMask::kConic) != 0;
  value["cubic"] = (masks & Path::SegmentMask::kCubic) != 0;
  return value;
}

}  // namespace

Json::Value BuildNormalizedPathJson(const Path& path,
                                    const PathNormalizeOptions& options) {
  Json::Value result(Json::objectValue);
  result["mode"] = "path_normalized";
  result["epsilon"] = options.epsilon;
  result["fill_type"] = FillTypeToString(path.GetFillType());
  result["empty"] = path.IsEmpty();
  result["finite"] = path.IsFinite();
  result["source_point_count"] = static_cast<Json::UInt64>(path.CountPoints());
  result["source_verb_count"] = static_cast<Json::UInt64>(path.CountVerbs());
  Json::Value bounds = RectToJson(path.GetBounds(), options.epsilon);
  result["bounds"] = bounds;
  result["source_bounds"] = std::move(bounds);
  result["segment_masks"] = SegmentMasksToJson(path.GetSegmentMasks());

  Json::Value normalization(Json::objectValue);
  normalization["drop_zero_length_segments"] =
      options.drop_zero_length_segments;
  normalization["normalize_negative_zero"] = true;

  Json::Value verbs(Json::arrayValue);
  Json::Value contours(Json::arrayValue);
  Json::Value sequence(Json::arrayValue);
  ContourState contour;
  Path::RawIter iter(path);
  Point points[4];
  const float* conic_weights = path.ConicWeights();
  Json::UInt64 serialized_point_count = 0;
  Json::UInt64 dropped_zero_length_segments = 0;

  for (Path::Verb verb = iter.Next(points); verb != Path::Verb::kDone;
       verb = iter.Next(points)) {
    const int point_count = SerializedPointCount(verb);
    std::vector<NormalizedPoint> normalized_points;
    normalized_points.reserve(point_count);
    for (int i = 0; i < point_count; ++i) {
      normalized_points.push_back(NormalizePoint(points[i], options.epsilon));
    }

    double conic_weight = 0.0;
    if (verb == Path::Verb::kConic) {
      conic_weight = *conic_weights++;
    }

    if (options.drop_zero_length_segments &&
        IsZeroLengthSegment(verb, normalized_points)) {
      dropped_zero_length_segments += 1;
      continue;
    }

    const Json::UInt64 verb_index = verbs.size();
    Json::Value item(Json::objectValue);
    item["index"] = verb_index;
    item["verb"] = VerbToString(verb);
    item["point_count"] = point_count;

    Json::Value json_points(Json::arrayValue);
    for (const auto& point : normalized_points) {
      json_points.append(PointToJson(point, options.epsilon));
    }
    item["points"] = std::move(json_points);
    serialized_point_count += static_cast<Json::UInt64>(point_count);

    if (verb == Path::Verb::kConic) {
      item["weight"] = ScalarToJson(conic_weight, options.epsilon);
    }

    AddVerbToContour(verb, normalized_points, verb_index, &contours, &contour,
                     options.epsilon);
    sequence.append(VerbToString(verb));
    verbs.append(std::move(item));
  }
  FinishContour(&contour, &contours, options.epsilon);

  normalization["dropped_zero_length_segments"] = dropped_zero_length_segments;
  result["normalization"] = std::move(normalization);
  result["verb_count"] = static_cast<Json::UInt64>(verbs.size());
  result["point_count"] = serialized_point_count;
  result["contour_count"] = static_cast<Json::UInt64>(contours.size());
  result["verb_sequence"] = std::move(sequence);
  result["verbs"] = std::move(verbs);
  result["contours"] = std::move(contours);
  return result;
}

}  // namespace font_harness
}  // namespace skity
