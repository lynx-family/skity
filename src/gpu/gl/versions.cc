// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include "src/gpu/gl/versions.hpp"

#include <sstream>
#include <string>
#include <vector>

#include "src/gpu/gl/gl_interface.hpp"

namespace skity {

static std::string GetGLString(const GLInterface*, GLenum name) {
  auto str = GL_CALL(GetString, name);
  if (str == nullptr) {
    return "";
  }
  return reinterpret_cast<const char*>(str);
}

bool HasPrefix(const std::string& string, const std::string& prefix) {
  return string.find(prefix) == 0u;
}

Versions::Versions(const GLInterface* interface) {
  if (!interface) {
    Fallback();
    return;
  }
  auto version_str = GetGLString(interface, GL_VERSION);
  if (version_str.empty()) {
    Fallback();
    return;
  }
  ResolveVersions(version_str);
}

void Versions::ResolveVersions(const std::string version_str) {
  if (!version_str.empty() && HasPrefix(version_str, "OPENGL ES")) {
    is_es_ = true;
  } else {
    is_es_ = false;
  }

  std::stringstream stream;
  bool start_digit = false;
  for (size_t i = 0; i < version_str.size(); i++) {
    const auto character = version_str[i];
    if (std::isdigit(character) || character == '.') {
      start_digit = true;
      stream << character;
    } else {
      if (start_digit) {
        break;
      }
    }
  }
  std::istringstream istream;
  istream.str(stream.str());
  std::vector<size_t> version_components;
  for (std::string version_component;
       std::getline(istream, version_component, '.');) {
    version_components.push_back(std::stoul(version_component));
  }
  if (version_components.size() == 0) {
    Fallback();
  } else {
    if (version_components.size() > 0) {
      gl_major_ = version_components[0];
    }
    if (version_components.size() > 1) {
      gl_minor_ = version_components[1];
    }
  }
}

void Versions::Fallback() {
  // fallback to OPENGL ES 2.0
  gl_major_ = 2;
  gl_minor_ = 0;
  is_es_ = true;
}

}  // namespace skity
