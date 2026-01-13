// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include <array>
#include <skity/skity.hpp>
// serialize module
#include <skity/io/picture.hpp>
#include <skity/io/stream.hpp>

using namespace skity;

int main(int argc, char const *argv[]) {
  PictureRecorder recorder;

  recorder.BeginRecording();

  auto canvas = recorder.GetRecordingCanvas();

  Paint paint;
  std::array<Point, 2> points = {
      Point{0.f, 0.f, 0.f, 1.f},
      {100.f, 100.f, 0.f, 1.f},
  };

  std::array<Vec4, 3> colors = {
      Vec4{1.f, 0.f, 0.f, 1.f},
      Vec4{0.f, 1.f, 0.f, 1.f},
      Vec4{0.f, 0.f, 1.f, 1.f},
  };

  std::array<float, 3> offset{0.f, 0.5f, 1.f};

  auto shader = Shader::MakeRadial(Point{50.f, 50.f, 0.f, 1.f}, 50.f,
                                   colors.data(), offset.data(), colors.size(),
                                   TileMode::kClamp);

  paint.SetShader(shader);

  canvas->DrawRect(Rect::MakeLTRB(0.f, 0.f, 100.f, 100.f), paint);

  auto dl = recorder.FinishRecording();

  auto picture = Picture::MakeFromDisplayList(dl.get());

  auto stream = WriteStream::CreateFileStream("linear-gradient.skp");

  picture->Serialize(*stream, nullptr);

  stream->Flush();

  return 0;
}
