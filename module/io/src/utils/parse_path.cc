/*
 * Copyright 2011 Google Inc.
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <skity/io/parse_path.hpp>
#include <skity/skity.hpp>
#include <sstream>

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
  float v = (float)strtod(str, &stop);
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
  Path path;
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
        op = (char)to_upper(op);
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
        path.MoveTo(Vec4{points[0], 0, 1});
        previousOp = '\0';
        op = 'L';
        c = points[0];
        break;
      case 'L':  // Line
        data = find_points(data, points, 1, relative, &c);
        path.LineTo(Vec4{points[0], 0, 1});
        c = points[0];
        break;
      case 'H':  // Horizontal Line
        data = find_scalar(data, &scratch, relative, c.x);
        // Similarly, if there wasn't a scalar to read, data will
        // be set to nullptr and this lineTo is bogus but will
        // be ultimately ignored when the next time through the loop
        // detects that and bails out.
        path.LineTo(scratch, c.y);
        c.x = scratch;
        break;
      case 'V':  // Vertical Line
        data = find_scalar(data, &scratch, relative, c.y);
        path.LineTo(c.x, scratch);
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
        path.CubicTo(Vec4{points[0], 0, 1}, Vec4{points[1], 0, 1},
                     Vec4{points[2], 0, 1});
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
        path.QuadTo(Vec4{points[0], 0, 1}, Vec4{points[1], 0, 1});
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
          path.ArcTo(radii.x, radii.y, angle, (Path::ArcSize)largeArc,
                     (Path::Direction)!sweep, points[0].x, points[0].y);
          Point last_pt;
          path.GetLastPt(&last_pt);
          c = last_pt.xy();
        }
      } break;
      case 'Z':  // Close Path
        path.Close();
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

  return path;
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

  Paint paint;
  Stroke stroke{paint};
  Path dst_path;
  stroke.QuadPath(path, &dst_path, true);

  Path::Iter iter(dst_path, false);
  Point pts[4];
  for (;;) {
    auto verb = iter.Next(pts);
    switch (verb) {
      case Path::Verb::kMove:
        append_command('M', &pts[0], 1);
        break;
      case Path::Verb::kLine:
        append_command('L', &pts[1], 1);
        break;
      case Path::Verb::kQuad:
        append_command('Q', &pts[1], 2);
        break;
      case Path::Verb::kCubic:
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

    if (verb == Path::Verb::kDone) {
      break;
    }
  }

  std::string str = ss.str();
  return str;
}
}  // namespace skity
