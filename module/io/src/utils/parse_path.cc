/*
 * Copyright 2011 Google Inc.
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include "src/utils/parse_path.hpp"

#include <skity/c/skity_stroke.h>

#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <skity/skity.hpp>
#include <sstream>

#include "src/io/core_bridge.hpp"

namespace skity {

static inline bool is_between(int c, int min, int max) {
  return (unsigned)(c - min) <= (unsigned)(max - min);
}

static inline bool is_ws(int c) { return is_between(c, 1, 32); }

static inline bool is_digit(int c) { return is_between(c, '0', '9'); }

static inline bool is_sep(int c) { return is_ws(c) || c == ','; }

static inline bool is_lower(int c) { return is_between(c, 'a', 'z'); }

static inline int to_upper(int c) { return c - 'a' + 'A'; }

static const char* skip_ws(const char str[]) {
  assert(str);
  while (is_ws(*str)) str++;
  return str;
}

static const char* skip_sep(const char str[]) {
  if (!str) {
    return nullptr;
  }
  while (is_sep(*str)) str++;
  return str;
}

const char* FindScalar(const char str[], float* value) {
  assert(str);
  str = skip_ws(str);

  char* stop;
  float v = static_cast<float>(strtod(str, &stop));
  if (str == stop) {
    return nullptr;
  }
  if (value) {
    *value = v;
  }
  return stop;
}

const char* FindScalars(const char str[], float value[], int count) {
  assert(count >= 0);

  if (count > 0) {
    for (;;) {
      str = FindScalar(str, value);
      if (--count == 0 || str == nullptr) break;

      // keep going
      str = skip_sep(str);
      if (value) value += 1;
    }
  }
  return str;
}

// If unable to read count points from str into value, this will return nullptr
// to signal the failure. Otherwise, it will return the next offset to read
// from.
static const char* find_points(const char str[], Vec2 value[], int count,
                               bool isRelative, Vec2* relative) {
  str = FindScalars(str, &value[0].x, count * 2);
  if (isRelative) {
    for (int index = 0; index < count; index++) {
      value[index].x += relative->x;
      value[index].y += relative->y;
    }
  }
  return str;
}

// If unable to read a scalar from str into value, this will return nullptr
// to signal the failure. Otherwise, it will return the next offset to read
// from.
static const char* find_scalar(const char str[], float* value, bool isRelative,
                               float relative) {
  str = FindScalar(str, value);
  if (!str) {
    return nullptr;
  }
  if (isRelative) {
    *value += relative;
  }
  str = skip_sep(str);
  return str;
}

// https://www.w3.org/TR/SVG11/paths.html#PathDataBNF
//
// flag:
//    "0" | "1"
static const char* find_flag(const char str[], bool* value) {
  if (!str) {
    return nullptr;
  }
  if (str[0] != '1' && str[0] != '0') {
    return nullptr;
  }
  *value = str[0] != '0';
  str = skip_sep(str + 1);
  return str;
}

std::optional<Path> ParsePath::FromSVGString(const char data[]) {
  // We will write all data to this local path and only write it
  // to result if the whole parsing succeeds.
  core_bridge::PathBuilder builder;
  Vec2 first = {0, 0};
  Vec2 c = {0, 0};
  Vec2 lastc = {0, 0};
  // We will use find_points and find_scalar to read into these.
  // There might not be enough data to fill them, so to avoid
  // MSAN warnings about using uninitialized bytes, we initialize
  // them there.
  Vec2 points[3] = {};
  float scratch = 0;
  char op = '\0';
  char previousOp = '\0';
  bool relative = false;
  for (;;) {
    if (!data) {
      // Truncated data
      return {};
    }
    data = skip_ws(data);
    if (data[0] == '\0') {
      break;
    }
    char ch = data[0];
    if (is_digit(ch) || ch == '-' || ch == '+' || ch == '.') {
      if (op == '\0' || op == 'Z') {
        return {};
      }
    } else if (is_sep(ch)) {
      data = skip_sep(data);
    } else {
      op = ch;
      relative = false;
      if (is_lower(op)) {
        op = static_cast<char>(to_upper(op));
        relative = true;
      }
      data++;
      data = skip_sep(data);
    }
    switch (op) {
      case 'M':  // Move
        data = find_points(data, points, 1, relative, &c);
        // find_points might have failed, so this might be the
        // previous point. However, data will be set to nullptr
        // if it failed, so we will check this at the top of the loop.
        builder.MoveTo(points[0].x, points[0].y);
        previousOp = '\0';
        op = 'L';
        c = points[0];
        break;
      case 'L':  // Line
        data = find_points(data, points, 1, relative, &c);
        builder.LineTo(points[0].x, points[0].y);
        c = points[0];
        break;
      case 'H':  // Horizontal Line
        data = find_scalar(data, &scratch, relative, c.x);
        // Similarly, if there wasn't a scalar to read, data will
        // be set to nullptr and this lineTo is bogus but will
        // be ultimately ignored when the next time through the loop
        // detects that and bails out.
        builder.LineTo(scratch, c.y);
        c.x = scratch;
        break;
      case 'V':  // Vertical Line
        data = find_scalar(data, &scratch, relative, c.y);
        builder.LineTo(c.x, scratch);
        c.y = scratch;
        break;
      case 'C':  // Cubic Bezier Curve
        data = find_points(data, points, 3, relative, &c);
        goto cubicCommon;
      case 'S':  // Continued "Smooth" Cubic Bezier Curve
        data = find_points(data, &points[1], 2, relative, &c);
        points[0] = c;
        if (previousOp == 'C' || previousOp == 'S') {
          points[0].x -= lastc.x - c.x;
          points[0].y -= lastc.y - c.y;
        }
      cubicCommon:
        builder.CubicTo(points[0].x, points[0].y, points[1].x, points[1].y,
                        points[2].x, points[2].y);
        lastc = points[1];
        c = points[2];
        break;
      case 'Q':  // Quadratic Bezier Curve
        data = find_points(data, points, 2, relative, &c);
        goto quadraticCommon;
      case 'T':  // Continued Quadratic Bezier Curve
        data = find_points(data, &points[1], 1, relative, &c);
        points[0] = c;
        if (previousOp == 'Q' || previousOp == 'T') {
          points[0].x -= lastc.x - c.x;
          points[0].y -= lastc.y - c.y;
        }
      quadraticCommon:
        builder.QuadTo(points[0].x, points[0].y, points[1].x, points[1].y);
        lastc = points[0];
        c = points[1];
        break;
      case 'A': {  // Arc (Elliptical)
        Vec2 radii;
        float angle;
        bool largeArc, sweep;
        if ((data = find_points(data, &radii, 1, false, nullptr)) &&
            (data = skip_sep(data)) &&
            (data = find_scalar(data, &angle, false, 0)) &&
            (data = skip_sep(data)) && (data = find_flag(data, &largeArc)) &&
            (data = skip_sep(data)) && (data = find_flag(data, &sweep)) &&
            (data = skip_sep(data)) &&
            (data = find_points(data, &points[0], 1, relative, &c))) {
          builder.ArcTo(
              radii.x, radii.y, angle, static_cast<Path::ArcSize>(largeArc),
              static_cast<Path::Direction>(!sweep), points[0].x, points[0].y);
          // The next relative command continues from the arc's end point;
          // re-read it through the C ABI (get_last_pt).
          skity_path handle = skity::capi::alloc_handle<skity_path_s>(
              SKITY_OBJECT_TYPE_PATH, 0, builder.Peek());
          skity_point last_pt{};
          skity_path_get_last_pt(handle, &last_pt);
          skity_path_destroy(handle);
          c = {last_pt.x, last_pt.y};
        }
      } break;
      case 'Z':  // Close Path
        builder.Close();
        c = first;
        break;
      default:
        return {};
    }
    if (previousOp == 0) {
      first = c;
    }
    previousOp = op;
  }

  auto path = builder.Take();
  return path ? std::optional<Path>{*path} : std::nullopt;
}

// ///////////////////////////////////////////////////////////////////////////////

std::string ParsePath::ToSVGString(const Path& path, PathEncoding encoding) {
  std::stringstream ss;

  Vec4 current_point{0, 0, 0, 1};
  const auto rel_selector = encoding == PathEncoding::Relative;

  const auto append_command = [&](char cmd, const Point pts[], size_t count) {
    // Use lower case cmds for relative encoding.
    cmd += 32 * rel_selector;
    // stream.write(&cmd, 1);
    ss << cmd;

    for (size_t i = 0; i < count; ++i) {
      const auto pt = pts[i] - current_point;
      if (i > 0) {
        ss << " ";
      }
      ss << pt.x;
      ss << " ";
      ss << pt.y;
    }

    assert(count > 0);
    // For relative encoding, track the current point (otherwise == origin).
    current_point = pts[count - 1] * rel_selector;
  };

  // Stroke the path into its quad/cubic outline through the C ABI, then
  // walk the outline via the flat bulk readers (each verb contributes its
  // control points after the shared start point, matching Path::Iter).
  auto paint = core_bridge::MakePaint();

  skity_path src = skity::capi::alloc_handle<skity_path_s>(
      SKITY_OBJECT_TYPE_PATH, 0,
      std::shared_ptr<Path>(const_cast<Path*>(&path), [](Path*) {}));
  skity_path dst = skity_path_create();
  if (paint != nullptr) {
    skity_paint paint_handle_view = skity::capi::alloc_handle<skity_paint_s>(
        SKITY_OBJECT_TYPE_PAINT, SKITY_HANDLE_OWNING, paint);
    skity_stroke_quad_path(paint_handle_view, src, dst, 1u);
    skity_paint_destroy(paint_handle_view);
  }
  skity_path_destroy(src);

  std::vector<skity_path_verb> verbs(skity_path_get_verbs(dst, nullptr, 0));
  std::vector<skity_point> points(skity_path_get_points(dst, nullptr, 0));
  skity_path_get_verbs(dst, verbs.data(), static_cast<int32_t>(verbs.size()));
  skity_path_get_points(dst, points.data(),
                        static_cast<int32_t>(points.size()));
  skity_path_destroy(dst);

  Point pts[4];
  size_t pt = 0;
  for (size_t vi = 0; vi < verbs.size(); vi++) {
    auto verb = static_cast<Path::Verb>(verbs[vi]);
    switch (verb) {
      case Path::Verb::kMove:
        pts[0] = {points[pt].x, points[pt].y, 0.f, 1.f};
        pt += 1;
        append_command('M', &pts[0], 1);
        break;
      case Path::Verb::kLine:
        pts[1] = {points[pt].x, points[pt].y, 0.f, 1.f};
        pt += 1;
        append_command('L', &pts[1], 1);
        break;
      case Path::Verb::kQuad:
        pts[1] = {points[pt].x, points[pt].y, 0.f, 1.f};
        pts[2] = {points[pt + 1].x, points[pt + 1].y, 0.f, 1.f};
        pt += 2;
        append_command('Q', &pts[1], 2);
        break;
      case Path::Verb::kCubic:
        pts[1] = {points[pt].x, points[pt].y, 0.f, 1.f};
        pts[2] = {points[pt + 1].x, points[pt + 1].y, 0.f, 1.f};
        pts[3] = {points[pt + 2].x, points[pt + 2].y, 0.f, 1.f};
        pt += 3;
        append_command('C', &pts[1], 3);
        break;
      case Path::Verb::kClose:
        ss << "Z";
        break;
      case Path::Verb::kConic:
        abort();
        break;
      default:
        break;
    }
  }

  std::string str = ss.str();
  return str;
}
}  // namespace skity
