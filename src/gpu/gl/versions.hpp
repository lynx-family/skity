// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#ifndef SRC_GPU_GL_VERSIONS_HPP
#define SRC_GPU_GL_VERSIONS_HPP

#include <cstddef>
#include <string>

class VersionsTest;

namespace skity {

struct GLInterface;

class Versions {
 public:
  explicit Versions(const GLInterface* interface);

  size_t GLMajor() const { return gl_major_; }
  size_t GLMinor() const { return gl_minor_; }
  bool IsEs() const { return is_es_; }

  friend class ::VersionsTest;

 private:
  void ResolveVersions(const std::string version_str);

  void Fallback();

  size_t gl_major_ = 0;
  size_t gl_minor_ = 0;
  bool is_es_ = false;
};

}  // namespace skity

#endif  // SRC_GPU_GL_VERSIONS_HPP
