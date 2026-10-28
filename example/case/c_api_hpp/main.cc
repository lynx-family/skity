// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

//
// The unified-shape C++ example: the classic "basic" demo (shapes / path
// effects / gradients / text) written against the header-only RAII wrapper
// (<skity/...>), running on the C-ABI-only unified libskity.
//
// The GLFW + OpenGL bootstrap mirrors case/c_api/main.c; the drawing is
// example/case/basic/example.cc ported nearly verbatim onto the wrapper —
// same names, same signatures, RAII ownership instead of manual destroy.
// The static scene is recorded into a DisplayList once and replayed every
// frame (the display-list use case). Pass `--frames N` to exit after N
// rendered frames (window runs until closed without it) and `--save F` to
// write the last frame to a PNG through the codec C ABI — the documented
// interim path for unified C++ consumers until codec grows a wrapper.
//

#include <GLFW/glfw3.h>
#include <skity/c/skity_codec.h>
#include <skity/skity.hpp>

#include <cmath>
#include <cstdio>
#include <cstring>

namespace {

void* GetGLProc(const char* name) {
  return reinterpret_cast<void*>(glfwGetProcAddress(name));
}

// Encode the scene to PNG: replay the SAME DisplayList through a software
// canvas — record once, replay on any backend (the GL backend exposes no
// GPU-to-CPU readback today, and a CPU replay is deterministic). The
// skity-data / skity-pixmap handles are shared across libskity and
// libskity-codec, so the RAII layer interops with the raw codec ABI.
bool SavePNG(const skity::DisplayList& scene, const char* path,
             uint32_t width, uint32_t height) {
  skity::Bitmap bitmap(width, height);
  skity::SoftwareCanvas canvas(bitmap.get());
  canvas.DrawColor(skity::Color_WHITE, skity::BlendMode::kSrc);
  scene.Draw(canvas);
  canvas.Flush();

  skity::Pixmap pixels = bitmap.GetPixmap();
  skity_data png = skity_codec_encode(pixels.get(), SKITY_CODEC_TYPE_PNG);
  if (png == nullptr) {
    std::fprintf(stderr, "skity_codec_encode failed\n");
    return false;
  }
  std::FILE* file = std::fopen(path, "wb");
  if (file == nullptr) {
    std::fprintf(stderr, "fopen(%s) failed\n", path);
    skity_data_destroy(png);
    return false;
  }
  size_t size = skity_data_get_size(png);
  size_t written = std::fwrite(skity_data_get_data(png), 1, size, file);
  std::fclose(file);
  skity_data_destroy(png);
  std::printf("saved %zu bytes to %s\n", size, path);
  return written == size;
}

float ContentScale(GLFWwindow* window) {
  int fbw = 0, fbh = 0, winw = 0, winh = 0;
  glfwGetFramebufferSize(window, &fbw, &fbh);
  glfwGetWindowSize(window, &winw, &winh);
  if (winw <= 0 || winh <= 0 || fbw <= 0 || fbh <= 0) {
    return 1.f;
  }
  return std::sqrt(static_cast<float>(fbw * fbw + fbh * fbh) /
                   static_cast<float>(winw * winw + winh * winh));
}

// ---- drawing: example/case/basic/example.cc ported onto the wrapper ----

// same as https://fiddle.skia.org/c/@shapes
void DrawBasicExample(skity::Canvas* canvas) {
  skity::Paint paint;
  paint.SetStyle(skity::Paint::kFill_Style);
  paint.SetAntiAlias(true);
  paint.SetStrokeWidth(4.f);
  paint.SetFillColor(0x42 / 255.f, 0x85 / 255.f, 0xF4 / 255.f, 1.f);

  skity::Rect rect = skity::Rect::MakeXYWH(10, 10, 100, 160);
  canvas->DrawRect(rect, paint);

  skity::RRect oval;
  oval.SetOval(rect);
  oval.Offset(40, 80);
  paint.SetFillColor(0xDB / 255.f, 0x44 / 255.f, 0x37 / 255.f, 1.f);
  canvas->DrawRRect(oval, paint);

  paint.SetFillColor(0x0F / 255.f, 0x9D / 255.f, 0x58 / 255.f, 1.f);
  canvas->DrawCircle(180, 50, 25, paint);

  rect.Offset(80, 50);
  paint.SetStrokeColor(0xF4 / 255.f, 0xB4 / 255.f, 0x0, 1.f);
  paint.SetStyle(skity::Paint::kStroke_Style);
  canvas->DrawRoundRect(rect, 10, 10, paint);
}

// same as https://fiddle.skia.org/c/@discrete_path
void DrawPathEffectExample(skity::Canvas* canvas) {
  const float R = 115.2f, C = 128.f;
  skity::Path path;
  path.MoveTo(C + R, C);
  for (int32_t i = 1; i < 8; i++) {
    float a = 2.6927937f * i;
    path.LineTo(C + R * std::cos(a), C + R * std::sin(a));
  }

  skity::Paint paint;
  paint.SetPathEffect(skity::PathEffect::MakeDiscretePathEffect(10.f, 4.f));
  paint.SetStyle(skity::Paint::kStroke_Style);
  paint.SetStrokeWidth(2.f);
  paint.SetAntiAlias(true);
  paint.SetStrokeColor(0x42 / 255.f, 0x85 / 255.f, 0xF4 / 255.f, 1.f);
  canvas->DrawPath(path, paint);
}

void DrawDashStarExample(skity::Canvas* canvas) {
  skity::Path path;
  path.MoveTo(199, 34);
  path.LineTo(253, 143);
  path.LineTo(374, 160);
  path.LineTo(287, 244);
  path.LineTo(307, 365);
  path.LineTo(199, 309);
  path.LineTo(97, 365);
  path.LineTo(112, 245);
  path.LineTo(26, 161);
  path.LineTo(146, 143);
  path.Close();

  skity::Paint paint;
  paint.SetStrokeWidth(8.f);
  paint.SetStrokeJoin(skity::Paint::kRound_Join);
  paint.SetStrokeCap(skity::Paint::kRound_Cap);
  paint.SetStrokeColor(0, 0, 1, 1);
  paint.SetFillColor(150.f / 255.f, 150.f / 255.f, 1.f, 1.f);
  paint.SetAntiAlias(true);
  paint.SetStyle(skity::Paint::kStrokeAndFill_Style);
  float pattern[2] = {0.f, 20.f};
  paint.SetPathEffect(skity::PathEffect::MakeDashPathEffect(pattern, 2, 0));

  canvas->DrawPath(path, paint);
}

// same as https://fiddle.skia.org/c/844ab7d5e63876f6c889b33662ece8d5
void DrawLinearGradientExample(skity::Canvas* canvas) {
  skity::Paint p;
  p.SetStyle(skity::Paint::kFill_Style);

  skity::Color4f colors[] = {
      {0.f, 1.f, 1.f, 0.f},
      {0.f, 0.f, 1.f, 1.f},
      {1.f, 0.f, 0.f, 1.f},
  };
  float positions[] = {0.f, 0.65f, 1.f};

  for (int i = 0; i < 4; i++) {
    float blockX = (i % 2) * 100.f;
    float blockY = (i / 2) * 100.f;

    skity::Point pts[2] = {
        {blockX, blockY, 0.f, 1.f},
        {blockX + 50, blockY + 100, 0.f, 1.f},
    };

    skity::Matrix matrix;
    int flag = (i % 2 == 1) ? 1 : 0;
    if (i / 2 == 1) {
      matrix = skity::Matrix::RotateDeg(45.f, skity::Vec2{blockX, blockY});
    }
    auto lgs = skity::Shader::MakeLinear(pts, colors, positions, 3,
                                         skity::TileMode::kMirror, flag);
    lgs.SetLocalMatrix(matrix);
    p.SetShader(lgs);
    auto r = skity::Rect::MakeLTRB(blockX, blockY, blockX + 100, blockY + 100);
    canvas->DrawRect(r, p);
  }

  skity::Path circle;
  circle.AddCircle(220, 350, 100);
  skity::Paint paint;
  paint.SetStyle(skity::Paint::kFill_Style);
  paint.SetAntiAlias(true);
  skity::Point center{220, 350, 0, 1};
  skity::Color4f radialColors[] = {{1.f, 1.f, 1.f, 1.f}, {0.f, 0.f, 0.f, 1.f}};
  auto rgs = skity::Shader::MakeRadial(center, 150.f, radialColors, nullptr, 2);
  paint.SetShader(rgs);

  canvas->DrawPath(circle, paint);
}

// same as https://fiddle.skia.org/c/@text_rendering
void DrawSimpleText(skity::Canvas* canvas, const skity::Typeface& primary,
                    const skity::Typeface& cjk) {
  skity::Paint paint;

  paint.SetTextSize(64.f);
  paint.SetAntiAlias(true);
  paint.SetFillColor(0x42 / 255.f, 0x85 / 255.f, 0xF4 / 255.f, 1.f);
  paint.SetStyle(skity::Paint::kFill_Style);

  paint.SetTypeface(primary.get());
  // The legacy TextBlobBuilder + BuildTextBlob collapses onto TextBlob
  // factories on the wrapper: MakeWithDelegate threads the fallback through.
  // The fallback delegate stays alive for as long as the blob draws.
  auto draw_text = [&](const skity::TextBlob& blob) {
    canvas->DrawTextBlob(blob, 20.f, 64.f, paint);

    paint.SetStyle(skity::Paint::kStroke_Style);
    paint.SetStrokeColor(0xDB / 255.f, 0x44 / 255.f, 0x37 / 255.f, 1.f);
    paint.SetStrokeWidth(2.f);

    canvas->DrawTextBlob(blob, 20.f, 144.f, paint);

    paint.SetFillColor(0x0F / 255.f, 0x9D / 255.f, 0x58 / 255.f, 1.f);
    paint.SetStyle(skity::Paint::kFill_Style);

    canvas->Save();

    skity::Color4f colors[] = {
        {0.f, 1.f, 1.f, 1.f},
        {0.f, 0.f, 1.f, 1.f},
        {1.f, 0.f, 0.f, 1.f},
    };

    skity::Point pts[2] = {
        {0.f, 0.f, 0.f, 1.f},
        {200.f, 0.f, 0.f, 1.f},
    };

    auto lgs = skity::Shader::MakeLinear(pts, colors, nullptr, 3);
    paint.SetShader(lgs);

    canvas->DrawTextBlob(blob, 20.f, 224.f, paint);
    canvas->Restore();
  };
  if (cjk) {
    skity::TypefaceDelegate delegate =
        skity::TypefaceDelegate::CreateSimpleFallbackDelegate(&cjk, 1);
    draw_text(
        skity::TextBlob::MakeWithDelegate("Skity 你好", paint, delegate.get()));
  } else {
    draw_text(skity::TextBlob::Make("Skity 你好", paint));
  }
}

void DrawEvenOddFill(skity::Canvas* canvas) {
  skity::Paint paint;
  paint.SetStyle(skity::Paint::kFill_Style);
  paint.SetColor(skity::ColorSetA(skity::Color_RED, 64));
  paint.SetAntiAlias(true);

  skity::Path path;
  path.MoveTo(100, 10);
  path.LineTo(40, 180);
  path.LineTo(190, 60);
  path.LineTo(10, 60);
  path.LineTo(160, 180);
  path.Close();

  canvas->DrawPath(path, paint);

  canvas->Save();

  canvas->Translate(0, 200);

  path.SetFillType(skity::Path::PathFillType::kEvenOdd);
  canvas->DrawPath(path, paint);

  canvas->Restore();
}

void DrawScene(skity::Canvas* canvas, const skity::Typeface& primary,
               const skity::Typeface& cjk) {
  DrawBasicExample(canvas);

  canvas->Save();
  canvas->Translate(300, 0);
  DrawPathEffectExample(canvas);
  canvas->Restore();

  canvas->Save();
  canvas->Translate(0, 300);
  DrawDashStarExample(canvas);
  canvas->Restore();

  canvas->Save();
  canvas->Translate(520, 0);
  DrawSimpleText(canvas, primary, cjk);
  canvas->Restore();

  canvas->Save();
  canvas->Translate(400, 300);
  DrawLinearGradientExample(canvas);
  canvas->Restore();

  canvas->Save();
  canvas->Translate(800, 0);
  DrawEvenOddFill(canvas);
  canvas->Restore();
}

}  // namespace

int main(int argc, char** argv) {
  // `--frames N`: render N frames then exit (unattended runs); without it
  // the window stays open until closed. `--save F`: write the last rendered
  // frame to F as PNG.
  long frames = -1;
  const char* save_path = nullptr;
  for (int i = 1; i + 1 < argc; i += 2) {
    if (std::strcmp(argv[i], "--frames") == 0) {
      frames = std::atol(argv[i + 1]);
    } else if (std::strcmp(argv[i], "--save") == 0) {
      save_path = argv[i + 1];
    }
  }

  if (glfwInit() == GLFW_FALSE) {
    return 1;
  }

  glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
  glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
  glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
  glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GLFW_TRUE);

  GLFWwindow* window =
      glfwCreateWindow(1000, 800, "skity-hpp: unified C ABI + RAII wrapper",
                       nullptr, nullptr);
  if (window == nullptr) {
    glfwTerminate();
    return 1;
  }
  glfwMakeContextCurrent(window);

  // -- skity GL context (RAII: destroyed automatically in reverse order) --
  skity::Context context;
  if (skity::Context::CreateGL(&GetGLProc, &context) != SKITY_SUCCESS) {
    std::fprintf(stderr, "skity::Context::CreateGL failed\n");
    return 1;
  }

  // -- typefaces: explicit primary font, CJK fallback for 你好 --
  skity::Typeface primary = skity::Typeface::GetDefaultTypeface();
#ifdef SKITY_FONT_DIR
  if (!primary) {
    primary = skity::Typeface::MakeFromFile(
        SKITY_FONT_DIR "fonts/resources/Roboto-Regular.ttf");
  }
#endif
  skity::FontStyle normal_style{400, 5, SKITY_FONT_SLANT_UPRIGHT};
  skity::FontManager font_manager = skity::FontManager::RefDefault();
  skity::Typeface cjk = font_manager.MatchFamilyStyleCharacter(
      nullptr, normal_style, nullptr, 0, 0x7ECF /* 界 */);
#ifdef SKITY_FONT_DIR
  if (!cjk) {
    // No system CJK face available (e.g. macOS without the CoreText
    // backend): fall back to the bundled Noto Sans CJK collection.
    cjk = font_manager.MakeFromFile(
        SKITY_FONT_DIR "fonts/resources/NotoSansCJK-Regular.ttc", 0);
  }
#endif

  // -- record the static scene once, replay it every frame --
  skity::PictureRecorder recorder;
  recorder.BeginRecording(skity::Rect::MakeWH(1000.f, 800.f));
  skity::Canvas recording_canvas = recorder.GetRecordingCanvas();
  DrawScene(&recording_canvas, primary, cjk);
  skity::DisplayList scene = recorder.FinishRecording();
  if (!scene) {
    std::fprintf(stderr, "FinishRecording failed\n");
    return 1;
  }
  std::printf("recorded %u draw ops\n", scene.GetOpCount());

  // -- surface bound to the default framebuffer (gl_id = 0) --
  auto create_surface = [&](skity::Surface* out) {
    int fbw = 0, fbh = 0, winw = 0, winh = 0;
    glfwGetFramebufferSize(window, &fbw, &fbh);
    glfwGetWindowSize(window, &winw, &winh);

    skity_surface_create_info_gl gl_ext{};
    gl_ext.s_type = SKITY_STRUCTURE_TYPE_SURFACE_CREATE_INFO_GL;
    gl_ext.surface_type = SKITY_GL_SURFACE_TYPE_FRAMEBUFFER;
    gl_ext.gl_id = 0;
    gl_ext.has_stencil = 1;

    skity_surface_create_info info{};
    info.s_type = SKITY_STRUCTURE_TYPE_SURFACE_CREATE_INFO;
    info.p_next = &gl_ext;
    info.width = static_cast<uint32_t>(winw);
    info.height = static_cast<uint32_t>(winh);
    info.sample_count = 4;
    info.content_scale = ContentScale(window);

    skity::Surface::Create(context, info, out);
  };

  skity::Surface surface;
  create_surface(&surface);
  if (!surface) {
    std::fprintf(stderr, "skity::Surface::Create failed\n");
    return 1;
  }

  // -- render loop --
  uint32_t winw = 0, winh = 0;
  while (glfwWindowShouldClose(window) == GLFW_FALSE) {
    // Recreate the surface when the window size changes.
    if (surface.GetWidth() != winw || surface.GetHeight() != winh) {
      int cur_w = 0, cur_h = 0;
      glfwGetWindowSize(window, &cur_w, &cur_h);
      winw = static_cast<uint32_t>(cur_w);
      winh = static_cast<uint32_t>(cur_h);
      surface = skity::Surface();
      create_surface(&surface);
      if (!surface) {
        std::fprintf(stderr, "surface recreation failed\n");
        break;
      }
    }

    skity::Canvas canvas = surface.LockCanvas(true);

    canvas.DrawColor(skity::Color_WHITE, skity::BlendMode::kSrc);
    scene.Draw(canvas);

    canvas.Flush();
    surface.Flush();

    if (frames > 0 && frames == 1 && save_path != nullptr) {
      SavePNG(scene, save_path, surface.GetWidth(), surface.GetHeight());
    }

    glfwSwapBuffers(window);
    glfwPollEvents();

    if (frames > 0 && --frames == 0) {
      break;
    }
  }

  // Everything above (DisplayList, Surface, Context) releases through RAII;
  // the borrowing canvas views never owned their handles.
  glfwDestroyWindow(window);
  glfwTerminate();
  return 0;
}
