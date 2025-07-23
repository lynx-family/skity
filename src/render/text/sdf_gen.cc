// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include "src/render/text/sdf_gen.hpp"

#include <cmath>
#include <skity/geometry/point.hpp>

namespace skity {
namespace sdf {

constexpr uint8_t DF_PAD = 4;
constexpr float MAX_DIST = 2000.f;
constexpr Vec2 MAX_DIST_VEC{1000.f, 1000.f};
constexpr float SQRT2 = 1.41421354f;
constexpr float TOLERANCE = 1.f / (1 << 12);
constexpr uint8_t WIDTH = 4;
constexpr uint8_t MAGNIFICATION = 32;

namespace {

struct DFData {
  DFData(const Image<float>& image_in, const Image<uint8_t>& edges_in,
         const Image<Vec2>& gradients_in)
      : image(image_in),
        gradients(gradients_in),
        distance_vectors(image_in.GetWidth(), image_in.GetHeight()),
        distances(image_in.GetWidth(), image_in.GetHeight()) {}

  // image as input.
  const Image<float>& image;
  const Image<uint8_t> edges;
  // gradients is initialized at the beginning and not changed during df
  // computation.
  const Image<Vec2> gradients;
  // distance_vectors is auxiliary data for final distance.
  Image<Vec2> distance_vectors;
  // distances as output.
  Image<float> distances;
};

inline bool NearlyZero(float value, float tolerance = TOLERANCE) {
  return std::abs(value) < tolerance;
}

const Image<uint8_t> FoundEdges(const Image<float>& image) {
  const size_t h = image.GetHeight();
  const size_t w = image.GetWidth();

  Image<uint8_t> edges(w, h, 0);

  for (size_t y = 0; y < h - 1; ++y) {
    for (size_t x = 0; x < w - 1; ++x) {
      const float value = image.Get(x, y);
      if (value == 0.f) {
      } else if (value < 1.f) {
        edges.Set(x, y, 1);
      } else {
        if (x == 0 || y == 0 || x == w - 1 || y == h - 1) {
          edges.Set(x, y, 1);
        } else if (image.Get(x - 1, y - 1) == 0.f ||
                   image.Get(x, y - 1) == 0.f ||
                   image.Get(x + 1, y - 1) == 0.f ||
                   image.Get(x - 1, y) == 0.f || image.Get(x + 1, y) == 0.f ||
                   image.Get(x - 1, y + 1) == 0.f ||
                   image.Get(x, y + 1) == 0.f ||
                   image.Get(x + 1, y + 1) == 0.f) {
          edges.Set(x, y, 1);
        }
      }
    }
  }
  return edges;
}

// local gradient for edge pixels in image
const Image<Vec2> ComputeGradients(const Image<float>& image,
                                   const Image<uint8_t>& edges) {
  const size_t h = image.GetHeight();
  const size_t w = image.GetWidth();

  Image<Vec2> gradients(w, h, {0, 0});

  for (size_t y = 1; y < h - 1; ++y) {
    for (size_t x = 1; x < w - 1; ++x) {
      if (edges.Get(x, y)) {
        Vec2 gradient;
        gradient.x = image.Get(x + 1, y - 1) - image.Get(x - 1, y - 1) +
                     SQRT2 * image.Get(x + 1, y) - SQRT2 * image.Get(x - 1, y) +
                     image.Get(x + 1, y + 1) - image.Get(x - 1, y + 1);
        gradient.y = image.Get(x - 1, y + 1) - image.Get(x - 1, y - 1) +
                     SQRT2 * image.Get(x, y + 1) - SQRT2 * image.Get(x, y - 1) +
                     image.Get(x + 1, y + 1) - image.Get(x + 1, y - 1);
        gradients.Set(x, y, glm::normalize(gradient));
      }
    }
  }
  return gradients;
}

// computes the distance to an edge given an edge normal vector and a pixel's
// alpha value.
float EdgeDistance(float alpha, const Vec2& direction, Vec2& dist_vec) {
  float dist;
  if (NearlyZero(direction[0]) || NearlyZero(direction[1])) {
    dist = 0.5 - alpha;
  } else {
    Vec2 d{std::abs(direction[0]), std::abs(direction[1])};
    if (d[0] < d[1]) std::swap(d[0], d[1]);
    const float a1 = static_cast<float>(0.5 * d[1] / d[0]);
    if (alpha < a1) {
      // 0 <= a < a1.
      dist = 0.5 * (d[0] + d[1]) - sqrt(2.0 * d[0] * d[1] * alpha);
    } else if (alpha < 1.0 - a1) {
      // a1 <= a <= 1 - a1.
      dist = (0.5 - alpha) * d[0];
    } else {
      // 1 - a1 < a <= 1.
      dist = -0.5 * (d[0] + d[1]) + sqrt(2.0 * d[0] * d[1] * (1.f - alpha));
    }
  }
  dist_vec.x = dist * direction.x;
  dist_vec.y = dist * direction.y;
  return dist;
}

void InitDistance(const Image<float>& image, const Image<uint8_t>& edges,
                  const Image<Vec2>& gradients, Image<float>& distances,
                  Image<Vec2>& distance_vectors) {
  const size_t h = image.GetHeight();
  const size_t w = image.GetWidth();
  for (size_t y = 0; y < h; ++y) {
    for (size_t x = 0; x < w; ++x) {
      const float a = image.Get(x, y);
      float dist = MAX_DIST;
      Vec2 dist_vec = MAX_DIST_VEC;
      if (edges.Get(x, y)) {
        dist = EdgeDistance(a, gradients.Get(x, y), dist_vec);
      }
      // distance is absolute value
      distances.Set(x, y, std::abs(dist));
      distance_vectors.Set(x, y, dist_vec);
    }
  }
}

void Compare(DFData* data, glm::ivec2 cur_point, const glm::ivec2& offset) {
  float old_dist = data->distances.Get(cur_point.x, cur_point.y);
  const glm::ivec2 offset_point = cur_point + offset;
  Vec2 offset_dist_vec =
      data->distance_vectors.Get(offset_point.x, offset_point.y);
  Vec2 dist_vec;
  dist_vec.x = offset_dist_vec.x + offset.x;
  dist_vec.y = offset_dist_vec.y + offset.y;
  float new_dist = glm::length(dist_vec);
  if (new_dist < old_dist) {
    data->distances.Set(cur_point.x, cur_point.y, new_dist);
    data->distance_vectors.Set(cur_point.x, cur_point.y, dist_vec);
  }
}

void ComputeDistances(DFData* data) {
  // assume that image size is limited by texture size.
  const int16_t height = static_cast<int16_t>(data->image.GetHeight());
  const int16_t width = static_cast<int16_t>(data->image.GetWidth());

  // EDT pass 0
  for (int16_t y = 1; y < height; ++y) {
    // 4
    for (int16_t x = 1; x < width - 1; ++x) {
      // up
      Compare(data, glm::ivec2(x, y), glm::ivec2(0, -1));
      if (x > 0) {
        // left
        Compare(data, glm::ivec2(x, y), glm::ivec2(-1, 0));
        // up-left
        Compare(data, glm::ivec2(x, y), glm::ivec2(-1, -1));
      }
      if (x < width - 1) {
        // up-right
        Compare(data, glm::ivec2(x, y), glm::ivec2(1, -1));
      }
    }

    // 1
    for (int16_t x = width - 2; x >= 0; --x) {
      // right
      Compare(data, glm::ivec2(x, y), glm::ivec2(1, 0));
    }
  }

  // EDT pass 1
  for (int16_t y = height - 2; y >= 0; --y) {
    // 4
    for (int16_t x = 1; x < width - 1; ++x) {
      // left
      Compare(data, glm::ivec2(x, y), glm::ivec2(-1, 0));
    }

    // 1
    for (int16_t x = width - 2; x >= 0; --x) {
      // bottom
      Compare(data, glm::ivec2(x, y), glm::ivec2(0, 1));
      if (x > 0) {
        // bottom-left
        Compare(data, glm::ivec2(x, y), glm::ivec2(-1, 1));
      }
      if (x < width - 1) {
        // right
        Compare(data, glm::ivec2(x, y), glm::ivec2(1, 0));
        // bottom-right
        Compare(data, glm::ivec2(x, y), glm::ivec2(1, 1));
      }
    }
  }
}

// generate distance field image, ignoring sign.
const Image<float> GenerateDfImage(const Image<float>& image) {
  const Image<uint8_t> edges = FoundEdges(image);
  const Image<Vec2> gradients = ComputeGradients(image, edges);
  DFData data(image, edges, gradients);

  InitDistance(image, edges, gradients, data.distances, data.distance_vectors);

  ComputeDistances(&data);

  return data.distances;
}

const Image<float> ToFloatImage(const Image<uint8_t>& image) {
  const size_t w = image.GetWidth();
  const size_t h = image.GetHeight();
  Image<float> float_image(w, h);
  for (size_t y = 0; y < h; ++y) {
    for (size_t x = 0; x < w; ++x) {
      uint8_t alpha = image.Get(x, y);
      if (image.Get(x, y) == 0) {
        float_image.Set(x, y, 0.f);
      } else if (image.Get(x, y) == 255) {
        float_image.Set(x, y, 1.f);
      } else {
        float_image.Set(x, y, alpha * 0.00392156862f);
      }
    }
  }
  return float_image;
}

const Image<uint8_t> ToIntImage(const Image<uint8_t>& input_image,
                                const Image<float>& image) {
  const size_t w = image.GetWidth();
  const size_t h = image.GetHeight();
  Image<uint8_t> int_image(w, h);
  for (size_t y = 0; y < h; ++y) {
    for (size_t x = 0; x < w; ++x) {
      float dist = image.Get(x, y);
      if (input_image.Get(x, y) > 127) {
        dist = -dist;
      }
      dist = glm::clamp<float>(-dist, -WIDTH, WIDTH * 127.0f / 128.0f);
      dist += WIDTH;
      dist = dist * MAGNIFICATION;
      uint8_t int_dist = std::roundf(dist);
      int_image.Set(x, y, int_dist);
    }
  }
  return int_image;
}

}  // namespace

const Image<uint8_t> SdfGen::GenerateSdfImage(const Image<uint8_t>& src_image) {
  // Add padding
  const size_t src_width = src_image.GetWidth();
  const size_t src_height = src_image.GetHeight();
  const size_t padding_img_width = src_width + 2 * DF_PAD;
  const size_t padding_img_height = src_height + 2 * DF_PAD;
  Image<uint8_t> padding_image(padding_img_width, padding_img_height, 0);
  for (size_t y = 0; y < src_height; ++y) {
    for (size_t x = 0; x < src_width; ++x) {
      padding_image.Set(DF_PAD + x, DF_PAD + y, src_image.Get(x, y));
    }
  }

  // generate sdf
  const Image<float> sdf_image = GenerateDfImage(ToFloatImage(padding_image));
  Image<uint8_t> dst = ToIntImage(padding_image, sdf_image);
  return dst;
}

}  // namespace sdf
}  // namespace skity
