// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include <emscripten/bind.h>

#include <skity/effect/path_effect.hpp>
#include <skity/effect/shader.hpp>
#include <skity/geometry/rect.hpp>
#include <skity/graphic/paint.hpp>
#include <skity/graphic/path.hpp>
#include <skity/io/data.hpp>
#include <skity/render/canvas.hpp>
#include <skity/text/text_blob.hpp>
#include <skity/text/typeface.hpp>

#include "src/render/wasm/wasm_canvas.hpp"

using namespace emscripten;

// handle data convert
namespace skity {

std::unique_ptr<Canvas> CanvasMakeWebGLCanvas(std::string const& name,
                                              uint32_t width, uint32_t height);

std::shared_ptr<Data> MakeCopyWithString(std::string const& str) {
  if (str.empty()) {
    return nullptr;
  }

  return Data::MakeWithCopy(str.c_str(), str.length());
}

std::shared_ptr<Shader> MakeRadialShader(float cx, float cy, float radius,
                                         std::vector<uint32_t> const& colors) {
  if (colors.size() < 2) {
    return nullptr;
  }

  Point center{cx, cy, 0.f, 1.f};

  std::vector<Color4f> f_colors;

  for (auto c : colors) {
    f_colors.emplace_back(Color4fFromColor(c));
  }

  return Shader::MakeRadial(center, radius, f_colors.data(), nullptr,
                            f_colors.size());
}

std::shared_ptr<Shader> MakeLinearShader(float x1, float y1, float x2, float y2,
                                         std::vector<uint32_t> const& colors) {
  if (colors.size() < 2) {
    return nullptr;
  }

  std::array<skity::Point, 2> pts{skity::Point{x1, y1, 0.f, 1.f},
                                  skity::Point{x2, y2, 0.f, 1.f}};

  std::vector<Color4f> f_colors;

  for (auto c : colors) {
    f_colors.emplace_back(Color4fFromColor(c));
  }

  return Shader::MakeLinear(pts.data(), f_colors.data(), nullptr,
                            f_colors.size());
}

std::shared_ptr<Shader> MakeLinearShaderWithPos(
    float x1, float y1, float x2, float y2, std::vector<uint32_t> const& colors,
    std::vector<float> const& pos) {
  if (colors.size() < 2) {
    return nullptr;
  }

  if (colors.size() != pos.size()) {
    return nullptr;
  }

  std::array<skity::Point, 2> pts{skity::Point{x1, y1, 0.f, 1.f},
                                  skity::Point{x2, y2, 0.f, 1.f}};

  std::vector<Color4f> f_colors;

  for (auto c : colors) {
    f_colors.emplace_back(Color4fFromColor(c));
  }

  return Shader::MakeLinear(pts.data(), f_colors.data(), pos.data(),
                            f_colors.size());
}

std::shared_ptr<PathEffect> MakeDashEffect(std::vector<float> const& pattern) {
  return PathEffect::MakeDashPathEffect(pattern.data(), pattern.size(), 0);
}

skity::Matrix MatrixTranslate(skity::Matrix const& m, float x, float y) {
  return skity::Matrix(m).PreTranslate(x, y);
}

skity::Matrix MatrixRotate(skity::Matrix const& m, float angle, float x,
                           float y, float z) {
  return Matrix(m).PreRotate(angle, x, y);
}

skity::Matrix MatrixMultiply(skity::Matrix const& m1, skity::Matrix const& m2) {
  return m1 * m2;
}

}  // namespace skity

// canvas binding

EMSCRIPTEN_BINDINGS(skity) {
  register_vector<uint32_t>("VectorUint32");
  register_vector<float>("VectorFloat");

  class_<skity::Matrix>("Matrix")
      .constructor<>()
      .class_function("Translate", &skity::MatrixTranslate)
      .class_function("Rotate", &skity::MatrixRotate)
      .class_function("Multiply", &skity::MatrixMultiply);

  class_<skity::Shader>("Shader")
      .smart_ptr<std::shared_ptr<skity::Shader>>("Shader")
      .function("setLocalMatrix", &skity::Shader::SetLocalMatrix)
      .class_function("MakeLinear", &skity::MakeLinearShader)
      .class_function("MakeLinearWithPos", &skity::MakeLinearShaderWithPos)
      .class_function("MakeRadial", &skity::MakeRadialShader);

  class_<skity::Data>("Data")
      .smart_ptr<std::shared_ptr<skity::Data>>("Data")
      .property("size", &skity::Data::Size)
      .class_function("MakeWithCopy", &skity::MakeCopyWithString);

  class_<skity::PathEffect>("PathEffect")
      .smart_ptr<std::shared_ptr<skity::PathEffect>>("PathEffect")
      .class_function("MakeDiscretePathEffect",
                      &skity::PathEffect::MakeDiscretePathEffect)
      .class_function("MakeDashEffect", &skity::MakeDashEffect);

  class_<skity::TextBlob>("TextBlob")
      .smart_ptr<std::shared_ptr<skity::TextBlob>>("TextBlob");

  class_<skity::TextBlobBuilder>("TextBlobBuilder")
      .constructor()
      .function("BuildTextBlob",
                select_overload<std::shared_ptr<skity::TextBlob>(
                    std::string const&, skity::Paint const&)>(
                    &skity::TextBlobBuilder::BuildTextBlob));

  class_<skity::Rect>("Rect")
      .constructor()
      .constructor<float, float, float, float>()
      .class_function("MakeXYWH", &skity::Rect::MakeXYWH)
      .class_function("MakeWH", &skity::Rect::MakeWH)
      .class_function("MakeLTRB", &skity::Rect::MakeLTRB)
      .function("setLTRB", &skity::Rect::SetLTRB)
      .function("offset", &skity::Rect::Offset)
      .property("left", &skity::Rect::Left)
      .property("right", &skity::Rect::Right)
      .property("top", &skity::Rect::Top)
      .property("bottom", &skity::Rect::Bottom);

  class_<skity::RRect>("RRect")
      .constructor()
      .function("setRect", &skity::RRect::SetRect)
      .function("setRectXY", &skity::RRect::SetRectXY)
      .function("setOval", &skity::RRect::SetOval)
      .function("offset", &skity::RRect::Offset);

  enum_<skity::Paint::Style>("Style")
      .value("Fill", skity::Paint::kFill_Style)
      .value("Stroke", skity::Paint::kStroke_Style);

  enum_<skity::Paint::Cap>("LineCap")
      .value("Round", skity::Paint::kRound_Cap)
      .value("Butt", skity::Paint::kButt_Cap)
      .value("Square", skity::Paint::kSquare_Cap);

  enum_<skity::Paint::Join>("LineJoin")
      .value("Round", skity::Paint::kRound_Join)
      .value("Mitter", skity::Paint::kMiter_Join)
      .value("Miter", skity::Paint::kMiter_Join);

  enum_<skity::Path::Direction>("PathDirection")
      .value("CW", skity::Path::Direction::kCW)
      .value("CCW", skity::Path::Direction::kCCW);

  enum_<skity::Path::PathFillType>("PathFillType")
      .value("Winding", skity::Path::PathFillType::kWinding)
      .value("EvenOdd", skity::Path::PathFillType::kEvenOdd);

  function("ColorSetARGB", &skity::ColorSetARGB);

  class_<skity::Path>("Path")
      .constructor()
      .function("setFillType", &skity::Path::SetFillType)
      .function("MoveTo", select_overload<skity::Path&(float, float)>(
                              &skity::Path::MoveTo))
      .function("LineTo", select_overload<skity::Path&(float, float)>(
                              &skity::Path::LineTo))
      .function("QuadTo",
                select_overload<skity::Path&(float, float, float, float)>(
                    &skity::Path::QuadTo))
      .function("addCircle", &skity::Path::AddCircle)
      .function("close", &skity::Path::Close);

  class_<skity::Typeface>("Typeface")
      .class_function("MakeFromData", &skity::Typeface::MakeFromData,
                      allow_raw_pointers());

  class_<skity::Paint>("Paint")
      .constructor()
      .function("setStyle", &skity::Paint::SetStyle)
      .function("setStrokeWidth", &skity::Paint::SetStrokeWidth)
      .function("setStrokeJoin", &skity::Paint::SetStrokeJoin)
      .function("setStrokeCap", &skity::Paint::SetStrokeCap)
      .function("setColor", &skity::Paint::SetColor)
      .function("setTypeface", &skity::Paint::SetTypeface, allow_raw_pointers())
      .function("setTextSize", &skity::Paint::SetTextSize)
      .function("setShader", &skity::Paint::SetShader)
      .function("setPathEffect", &skity::Paint::SetPathEffect);

  class_<skity::WasmCanvas>("Canvas")
      .class_function("Make", &skity::WasmCanvas::Create)
      .function("save", &skity::WasmCanvas::Save)
      .function("translate", &skity::WasmCanvas::Translate)
      .function("restore", &skity::WasmCanvas::Restore)
      .function("drawRect", &skity::WasmCanvas::DrawRect)
      .function("drawPath", &skity::WasmCanvas::DrawPath)
      .function("drawRRect", &skity::WasmCanvas::DrawRRect)
      .function("drawRoundRect", &skity::WasmCanvas::DrawRoundRect)
      .function("drawCircle", &skity::WasmCanvas::DrawCircle)
      .function("drawTextBlob", &skity::WasmCanvas::DrawTextBlob)
      .function("flush", &skity::WasmCanvas::Flush);
}
