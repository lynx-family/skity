// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include "case/fake_3d/fake_3d_example.hpp"

#include <chrono>
#include <glm/gtc/matrix_transform.hpp>
#include <skity/geometry/quaternion.hpp>
#include <skity/skity.hpp>
#include <thread>

namespace skity::example::basic {

void draw_even_odd_fill(skity::Canvas* canvas) {
  std::this_thread::sleep_for(std::chrono::milliseconds(100));
  skity::Paint paint;
  paint.SetStyle(skity::Paint::kFill_Style);
  paint.SetColor(skity::ColorSetA(skity::Color_RED, 64));

  skity::Path path;
  path.MoveTo(100, 10);
  path.LineTo(40, 180);
  path.LineTo(190, 60);
  path.LineTo(10, 60);
  path.LineTo(160, 180);
  path.Close();

  static float t = 0.f;

  auto bounds = path.GetBounds();

  glm::vec2 anchor_point{bounds.CenterX(), bounds.CenterY()};

  auto proj = glm::perspective(180.f, 1.f, 0.001f, 1000.f);
  auto view =
      glm::lookAt(glm::vec3{0.f, 0.f, .5f}, {0.f, 0.f, 0.f}, {0.f, -1.f, 0.f});

  glm::mat4 orth = glm::ortho(-500.f, 500.f, -400.f, 400.f);
  glm::mat4 invOrth = glm::inverse(orth);

  glm::mat4 preM =
      glm::translate(glm::mat4(1.f), {-anchor_point.x, -anchor_point.y, 0.f});
  glm::mat4 midM_X =
      glm::rotate(glm::mat4(1.f), 0.9f * glm::pi<float>() * t, {1.f, 0.f, 0.f});
  glm::mat4 midM_Y =
      glm::rotate(glm::mat4(1.f), 0.5f * glm::pi<float>() * t, {0.f, 1.f, 0.f});
  glm::mat4 posM =
      glm::translate(glm::mat4(1.f), {anchor_point.x, anchor_point.y, 0.f});

  canvas->Save();

  auto result = posM * invOrth * proj * view * midM_X * midM_Y * orth * preM;
  canvas->Concat(reinterpret_cast<skity::Matrix&>(result));

  canvas->DrawPath(path, paint);

  canvas->Restore();

  canvas->Save();

  canvas->Translate(0, 200);

  static Quaternion start_q = Quaternion::FromXYZW(0, 0, 0, 1);
  static Quaternion end_q = Quaternion::FromEuler(0.9f * glm::pi<float>(),
                                                  0.5f * glm::pi<float>(), 0);
  // static Quaternion end_q =
  //     Quaternion::FromAxisAngle(Vec3(0, 1, 0), 2.0f * glm::pi<float>());
  const Quaternion progress = start_q.Slerp(end_q, t);

  Matrix rmidM = progress.ToMatrix();
  result = posM * invOrth * proj * view * reinterpret_cast<glm::mat4&>(rmidM) *
           orth * preM;
  canvas->Concat(reinterpret_cast<skity::Matrix&>(result));

  path.SetFillType(skity::Path::PathFillType::kEvenOdd);
  canvas->DrawPath(path, paint);

  canvas->Restore();

  t += 0.01f;

  if (t >= 1) {
    t = 0.f;
  }
}

void draw_fake3d(skity::Canvas* canvas) { draw_even_odd_fill(canvas); }

}  // namespace skity::example::basic
