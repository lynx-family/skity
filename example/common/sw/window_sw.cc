// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include "common/sw/window_sw.hpp"

#include <iostream>
#include <skity/skity.hpp>
#include <vector>

namespace skity {
namespace example {

typedef const char* (*S_PFNGLGETSTRINGPROC)(GLenum name);
typedef void (*S_PFNGLGENTEXTURESPROC)(GLsizei n, GLuint* textures);
typedef void (*S_PFNGLBINDTEXTUREPROC)(GLenum target, GLuint texture);
typedef void (*S_PFNGLTEXIMAGE2DPROC)(GLenum target, GLint level,
                                      GLint internalformat, GLsizei width,
                                      GLsizei height, GLint border,
                                      GLenum format, GLenum type,
                                      const GLvoid* data);
typedef void (*S_PFNGLTEXSUBIMAGE2DPROC)(GLenum target, GLint level,
                                         GLint xoffset, GLint yoffset,
                                         GLsizei width, GLsizei height,
                                         GLenum format, GLenum type,
                                         const GLvoid* data);
typedef void (*S_PFNGLTEXPARAMETERIPROC)(GLenum target, GLenum pname,
                                         GLint param);
typedef void (*S_PFNGLDELETETEXTURESPROC)(GLsizei n, const GLuint* textures);
typedef void (*S_PFNGLGENVERTEXARRAYSPROC)(GLsizei n, GLuint* arrays);
typedef void (*S_PFNGLBINDVERTEXARRAYPROC)(GLuint array);
typedef void (*S_PFNGLDELETEVERTEXARRAYSPROC)(GLsizei n, const GLuint* arrays);
typedef void (*S_PFNGLGENBUFFERSPROC)(GLsizei n, GLuint* buffers);
typedef void (*S_PFNGLBINDBUFFERPROC)(GLenum target, GLuint buffer);
typedef void (*S_PFNGLBUFFERDATAPROC)(GLenum target, GLsizeiptr size,
                                      const GLvoid* data, GLenum usage);
typedef void (*S_PFNGLBUFFERSUBDATAPROC)(GLenum target, GLintptr offset,
                                         GLsizeiptr size, const GLvoid* data);
typedef void (*S_PFNGLDELETEBUFFERSPROC)(GLsizei n, const GLuint* buffers);
typedef void (*S_PFNGLVERTEXATTRIBPOINTERPROC)(GLuint index, GLint size,
                                               GLenum type,
                                               GLboolean normalized,
                                               GLsizei stride,
                                               const GLvoid* pointer);
typedef void (*S_PFNGLENABLEVERTEXATTRIBARRAYPROC)(GLuint index);
typedef GLuint (*S_PFNGLCREATESHADERPROC)(GLenum type);
typedef void (*S_PFNGLSHADERSOURCEPROC)(GLuint shader, GLsizei count,
                                        const GLchar* const* string,
                                        const GLint* length);
typedef void (*S_PFNGLCOMPILESHADERPROC)(GLuint shader);
typedef void (*S_PFNGLGETSHADERIVPROC)(GLuint shader, GLenum pname,
                                       GLint* params);
typedef GLint (*S_PFNGLGETSHADERINFOLOGPROC)(GLuint shader, GLsizei bufSize,
                                             GLsizei* length, GLchar* infoLog);
typedef void (*S_PFNGLDELETEPROC)(GLuint shader);
typedef GLuint (*S_PFNGLCREATEPROGRAMPROC)();
typedef void (*S_PFNGLDELETEPROGRAMPROC)(GLuint program);
typedef void (*S_PFNGLUSEPROGRAMPROC)(GLuint program);
typedef void (*S_PFNGLATTACHSHADERPROC)(GLuint program, GLuint shader);
typedef void (*S_PFNGLLINKPROGRAMPROC)(GLuint program);
typedef void (*S_PFNGLGETPROGRAMIVPROC)(GLuint program, GLenum pname,
                                        GLint* params);
typedef GLint (*S_PFNGLGETPROGRAMINFOLOGPROC)(GLuint program, GLsizei bufSize,
                                              GLsizei* length, GLchar* infoLog);
typedef void (*S_PFNGLCLEARPROC)(GLbitfield mask);
typedef void (*S_PFNGLCLEARCOLORPROC)(GLclampf red, GLclampf green,
                                      GLclampf blue, GLclampf alpha);
typedef void (*S_PFNGLVIEWPORTPROC)(GLint x, GLint y, GLsizei width,
                                    GLsizei height);
typedef void (*S_PFNGLDRAWELEMENTSPROC)(GLenum mode, GLsizei count, GLenum type,
                                        const GLvoid* indices);

S_PFNGLGETSTRINGPROC fn_glGetString = nullptr;
S_PFNGLGENTEXTURESPROC fn_glGenTextures = nullptr;
S_PFNGLBINDTEXTUREPROC fn_glBindTexture = nullptr;
S_PFNGLTEXIMAGE2DPROC fn_glTexImage2D = nullptr;
S_PFNGLTEXSUBIMAGE2DPROC fn_glTexSubImage2D = nullptr;
S_PFNGLTEXPARAMETERIPROC fn_glTexParameteri = nullptr;
S_PFNGLDELETETEXTURESPROC fn_glDeleteTextures = nullptr;
S_PFNGLGENVERTEXARRAYSPROC fn_glGenVertexArrays = nullptr;
S_PFNGLBINDVERTEXARRAYPROC fn_glBindVertexArray = nullptr;
S_PFNGLDELETEVERTEXARRAYSPROC fn_glDeleteVertexArrays = nullptr;
S_PFNGLGENBUFFERSPROC fn_glGenBuffers = nullptr;
S_PFNGLBINDBUFFERPROC fn_glBindBuffer = nullptr;
S_PFNGLBUFFERDATAPROC fn_glBufferData = nullptr;
S_PFNGLBUFFERSUBDATAPROC fn_glBufferSubData = nullptr;
S_PFNGLDELETEBUFFERSPROC fn_glDeleteBuffers = nullptr;
S_PFNGLVERTEXATTRIBPOINTERPROC fn_glVertexAttribPointer = nullptr;
S_PFNGLENABLEVERTEXATTRIBARRAYPROC fn_glEnableVertexAttribArray = nullptr;
S_PFNGLCREATESHADERPROC fn_glCreateShader = nullptr;
S_PFNGLSHADERSOURCEPROC fn_glShaderSource = nullptr;
S_PFNGLCOMPILESHADERPROC fn_glCompileShader = nullptr;
S_PFNGLGETSHADERIVPROC fn_glGetShaderiv = nullptr;
S_PFNGLGETSHADERINFOLOGPROC fn_glGetShaderInfoLog = nullptr;
S_PFNGLDELETEPROC fn_glDeleteShader = nullptr;
S_PFNGLCREATEPROGRAMPROC fn_glCreateProgram = nullptr;
S_PFNGLDELETEPROGRAMPROC fn_glDeleteProgram = nullptr;
S_PFNGLUSEPROGRAMPROC fn_glUseProgram = nullptr;
S_PFNGLATTACHSHADERPROC fn_glAttachShader = nullptr;
S_PFNGLLINKPROGRAMPROC fn_glLinkProgram = nullptr;
S_PFNGLGETPROGRAMIVPROC fn_glGetProgramiv = nullptr;
S_PFNGLGETPROGRAMINFOLOGPROC fn_glGetProgramInfoLog = nullptr;
S_PFNGLCLEARPROC fn_glClear = nullptr;
S_PFNGLCLEARCOLORPROC fn_glClearColor = nullptr;
S_PFNGLVIEWPORTPROC fn_glViewport = nullptr;
S_PFNGLDRAWELEMENTSPROC fn_glDrawElements = nullptr;

void load_gl_function() {
  fn_glGetString = (S_PFNGLGETSTRINGPROC)glfwGetProcAddress("glGetString");
  fn_glGenTextures =
      (S_PFNGLGENTEXTURESPROC)glfwGetProcAddress("glGenTextures");
  fn_glBindTexture =
      (S_PFNGLBINDTEXTUREPROC)glfwGetProcAddress("glBindTexture");
  fn_glTexImage2D = (S_PFNGLTEXIMAGE2DPROC)glfwGetProcAddress("glTexImage2D");
  fn_glTexSubImage2D =
      (S_PFNGLTEXSUBIMAGE2DPROC)glfwGetProcAddress("glTexSubImage2D");
  fn_glTexParameteri =
      (S_PFNGLTEXPARAMETERIPROC)glfwGetProcAddress("glTexParameteri");
  fn_glDeleteTextures =
      (S_PFNGLDELETETEXTURESPROC)glfwGetProcAddress("glDeleteTextures");
  fn_glGenVertexArrays =
      (S_PFNGLGENVERTEXARRAYSPROC)glfwGetProcAddress("glGenVertexArrays");
  fn_glBindVertexArray =
      (S_PFNGLBINDVERTEXARRAYPROC)glfwGetProcAddress("glBindVertexArray");
  fn_glDeleteVertexArrays =
      (S_PFNGLDELETEVERTEXARRAYSPROC)glfwGetProcAddress("glDeleteVertexArrays");
  fn_glGenBuffers = (S_PFNGLGENBUFFERSPROC)glfwGetProcAddress("glGenBuffers");
  fn_glBindBuffer = (S_PFNGLBINDBUFFERPROC)glfwGetProcAddress("glBindBuffer");
  fn_glBufferData = (S_PFNGLBUFFERDATAPROC)glfwGetProcAddress("glBufferData");
  fn_glBufferSubData =
      (S_PFNGLBUFFERSUBDATAPROC)glfwGetProcAddress("glBufferSubData");
  fn_glDeleteBuffers =
      (S_PFNGLDELETEBUFFERSPROC)glfwGetProcAddress("glDeleteBuffers");
  fn_glVertexAttribPointer = (S_PFNGLVERTEXATTRIBPOINTERPROC)glfwGetProcAddress(
      "glVertexAttribPointer");
  fn_glEnableVertexAttribArray =
      (S_PFNGLENABLEVERTEXATTRIBARRAYPROC)glfwGetProcAddress(
          "glEnableVertexAttribArray");
  fn_glCreateShader =
      (S_PFNGLCREATESHADERPROC)glfwGetProcAddress("glCreateShader");
  fn_glShaderSource =
      (S_PFNGLSHADERSOURCEPROC)glfwGetProcAddress("glShaderSource");
  fn_glCompileShader =
      (S_PFNGLCOMPILESHADERPROC)glfwGetProcAddress("glCompileShader");
  fn_glGetShaderiv =
      (S_PFNGLGETSHADERIVPROC)glfwGetProcAddress("glGetShaderiv");
  fn_glGetShaderInfoLog =
      (S_PFNGLGETSHADERINFOLOGPROC)glfwGetProcAddress("glGetShaderInfoLog");
  fn_glDeleteShader = (S_PFNGLDELETEPROC)glfwGetProcAddress("glDeleteShader");
  fn_glCreateProgram =
      (S_PFNGLCREATEPROGRAMPROC)glfwGetProcAddress("glCreateProgram");
  fn_glDeleteProgram =
      (S_PFNGLDELETEPROGRAMPROC)glfwGetProcAddress("glDeleteProgram");
  fn_glUseProgram = (S_PFNGLUSEPROGRAMPROC)glfwGetProcAddress("glUseProgram");
  fn_glAttachShader =
      (S_PFNGLATTACHSHADERPROC)glfwGetProcAddress("glAttachShader");
  fn_glLinkProgram =
      (S_PFNGLLINKPROGRAMPROC)glfwGetProcAddress("glLinkProgram");
  fn_glGetProgramiv =
      (S_PFNGLGETPROGRAMIVPROC)glfwGetProcAddress("glGetProgramiv");
  fn_glGetProgramInfoLog =
      (S_PFNGLGETPROGRAMINFOLOGPROC)glfwGetProcAddress("glGetProgramInfoLog");

  fn_glClear = (S_PFNGLCLEARPROC)glfwGetProcAddress("glClear");
  fn_glClearColor = (S_PFNGLCLEARCOLORPROC)glfwGetProcAddress("glClearColor");
  fn_glViewport = (S_PFNGLVIEWPORTPROC)glfwGetProcAddress("glViewport");
  fn_glDrawElements =
      (S_PFNGLDRAWELEMENTSPROC)glfwGetProcAddress("glDrawElements");
}

bool WindowSW::OnInit() {
  glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
  glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
  glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
  glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);

  return true;
}

GLFWwindow* WindowSW::CreateWindowHandler() {
  auto window = glfwCreateWindow(GetWidth(), GetHeight(), GetTitle().c_str(),
                                 nullptr, nullptr);
  if (window == nullptr) {
    return nullptr;
  }
  glfwMakeContextCurrent(window);

  load_gl_function();

  // query gl version
  auto version = fn_glGetString(GL_VERSION);

  std::cout << "GL version: " << version << std::endl;

  fn_glGenTextures(1, &texture_);

  fn_glBindTexture(GL_TEXTURE_2D, texture_);
  fn_glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, GetWidth(), GetHeight(), 0,
                  GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
  fn_glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  fn_glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  fn_glBindTexture(GL_TEXTURE_2D, 0);

  {
    std::vector<float> vertices = {
        // [pos, uv]
        -1.0f, -1.0f, 0.0f, 0.0f,  // bottom left
        1.0f,  -1.0f, 1.0f, 0.0f,  // bottom right
        1.0f,  1.0f,  1.0f, 1.0f,  // top right
        -1.0f, 1.0f,  0.0f, 1.0f,  // top left
    };

    std::vector<uint32_t> indices = {
        0, 1, 2,  // first triangle
        2, 3, 0,  // second triangle
    };

    fn_glGenVertexArrays(1, &vao_);
    fn_glBindVertexArray(vao_);

    fn_glGenBuffers(1, &vbo_);
    fn_glBindVertexArray(vao_);
    fn_glBindBuffer(GL_ARRAY_BUFFER, vbo_);
    fn_glBufferData(
        GL_ARRAY_BUFFER,
        vertices.size() * sizeof(float) + indices.size() * sizeof(uint32_t),
        nullptr, GL_STATIC_DRAW);

    fn_glBufferSubData(GL_ARRAY_BUFFER, 0, vertices.size() * sizeof(float),
                       vertices.data());
    fn_glBufferSubData(GL_ARRAY_BUFFER, vertices.size() * sizeof(float),
                       indices.size() * sizeof(uint32_t), indices.data());

    fn_glEnableVertexAttribArray(0);
    fn_glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float),
                             nullptr);
    fn_glEnableVertexAttribArray(1);
    fn_glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float),
                             (void*)(2 * sizeof(float)));

    index_offset_ = vertices.size() * sizeof(float);

    fn_glBindBuffer(GL_ARRAY_BUFFER, 0);
    fn_glBindVertexArray(0);
  }

  {
    const char* vertex_shader_source = R"(
      #version 330 core
      layout (location = 0) in vec2 aPos;
      layout (location = 1) in vec2 aTexCoord;
      out vec2 TexCoord;
      void main()
      {
          gl_Position = vec4(aPos.x, aPos.y, 0.0, 1.0);
          TexCoord = vec2(aTexCoord.x, 1.0 - aTexCoord.y);
      }
    )";
    const char* fragment_shader_source = R"(
      #version 330 core
      out vec4 FragColor;
      in vec2 TexCoord;
      uniform sampler2D texture1;
      void main()
      {
          FragColor = texture(texture1, TexCoord);
      }
    )";

    uint32_t vertex_shader = fn_glCreateShader(GL_VERTEX_SHADER);
    fn_glShaderSource(vertex_shader, 1, &vertex_shader_source, nullptr);
    fn_glCompileShader(vertex_shader);
    int success;
    char info_log[512];
    fn_glGetShaderiv(vertex_shader, GL_COMPILE_STATUS, &success);

    if (!success) {
      fn_glGetShaderInfoLog(vertex_shader, 512, nullptr, info_log);
      std::cout << "ERROR::SHADER::VERTEX::COMPILATION_FAILED\n"
                << info_log << std::endl;

      return nullptr;
    }

    uint32_t fragment_shader = fn_glCreateShader(GL_FRAGMENT_SHADER);
    fn_glShaderSource(fragment_shader, 1, &fragment_shader_source, nullptr);
    fn_glCompileShader(fragment_shader);
    fn_glGetShaderiv(fragment_shader, GL_COMPILE_STATUS, &success);
    if (!success) {
      fn_glGetShaderInfoLog(fragment_shader, 512, nullptr, info_log);
      std::cout << "ERROR::SHADER::FRAGMENT::COMPILATION_FAILED\n"
                << info_log << std::endl;

      return nullptr;
    }
    program_ = fn_glCreateProgram();
    fn_glAttachShader(program_, vertex_shader);
    fn_glAttachShader(program_, fragment_shader);
    fn_glLinkProgram(program_);
    fn_glGetProgramiv(program_, GL_LINK_STATUS, &success);
    if (!success) {
      fn_glGetProgramInfoLog(program_, 512, nullptr, info_log);
      std::cout << "ERROR::SHADER::PROGRAM::LINKING_FAILED\n"
                << info_log << std::endl;
    }
    fn_glDeleteShader(vertex_shader);
    fn_glDeleteShader(fragment_shader);
  }

  return window;
}

std::unique_ptr<skity::GPUContext> WindowSW::CreateGPUContext() { return {}; }

void WindowSW::OnShow() {
  bitmap_ = std::make_unique<skity::Bitmap>(
      GetWidth(), GetHeight(), skity::AlphaType::kPremul_AlphaType);

  canvas_ = skity::Canvas::MakeSoftwareCanvas(bitmap_.get());
}

skity::Canvas* WindowSW::AquireCanvas() {
  canvas_->Clear(skity::Color_TRANSPARENT);

  canvas_->Save();

  canvas_->Scale(screen_scale_, screen_scale_);

  return canvas_.get();
}

void WindowSW::OnPresent() {
  canvas_->Restore();

  auto window = GetNativeWindow();

  int32_t pp_width, pp_height;
  glfwGetFramebufferSize(window, &pp_width, &pp_height);

  fn_glClearColor(0.f, 0.f, 0.f, 0.f);

  fn_glViewport(0, 0, pp_width, pp_height);

  fn_glClear(GL_COLOR_BUFFER_BIT);

  fn_glViewport(0, 0, GetWidth(), GetHeight());

  fn_glBindTexture(GL_TEXTURE_2D, texture_);
  fn_glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, bitmap_->Width(),
                     bitmap_->Height(), GL_RGBA, GL_UNSIGNED_BYTE,
                     bitmap_->GetPixelAddr());

  fn_glUseProgram(program_);
  fn_glBindVertexArray(vao_);
  fn_glBindBuffer(GL_ARRAY_BUFFER, vbo_);
  fn_glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, vbo_);

  fn_glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT,
                    (void*)(index_offset_ + 0 * sizeof(uint32_t)));

  glfwSwapBuffers(window);
}

void WindowSW::OnTerminate() {
  fn_glDeleteVertexArrays(1, &vao_);
  fn_glDeleteBuffers(1, &vbo_);
  fn_glDeleteProgram(program_);
  fn_glDeleteTextures(1, &texture_);

  canvas_.reset();
  bitmap_.reset();
}

}  // namespace example
}  // namespace skity
