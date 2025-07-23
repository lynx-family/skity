// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#ifndef INCLUDE_SKITY_GRAPHIC_PATH_MEASURE_HPP
#define INCLUDE_SKITY_GRAPHIC_PATH_MEASURE_HPP

#include <memory>
#include <skity/graphic/path.hpp>

namespace skity {

class ContourMeasure;
class ContourMeasureIter;

/**
 * @class PathMeasure
 *	Util class to measure path length
 */
class SKITY_API PathMeasure final {
 public:
  PathMeasure();
  /**
   * @brief Construct a new Path Measure object with specified path.
   *
   * @param path        the Path need to be measured
   * @param forceClosed insert close contour if needed
   * @param resScale    controls the precision of the measure. values > 1
   *                    increase the precision.
   */
  PathMeasure(Path const& path, bool forceClosed, float resScale = 1.f);

  ~PathMeasure();

  /**
   * @brief Reset this PathMeasure with specified path.
   *
   * @param path        the Path need to be measured.
   * @param forceClosed insert close contour if needed
   */
  void SetPath(Path const* path, bool forceClosed);

  /**
   * @brief the total length of the current contour, or 0 if no path is
   *        associated.
   *
   * @return total length of current contour
   */
  float GetLength();

  /**
   * @brief Get the Pos Tan object
   *
   * @param [in] distance   pins distance to 0 <= distance <= GetLength()
   * @param [out] position  current position in path coordinate.
   * @param [out] tangent   current tangent vector in path coordinate.
   * @return                return false if there is no path, or a zero-length
   *                        path was specified.
   */
  bool GetPosTan(float distance, Point* position, Vector* tangent);

  /**
   * @brief Given a start and stop distance. return the sub-path.
   *  If the segment is zero-length, return false.
   *  if startD > stopD, return false.
   *
   * @param startD          start distance of target segment
   * @param stopD           stop distance of target segment
   * @param [out] dst       sub-path between start and stop distance.
   * @param startWithMoveTo insert MoveTo if needed.
   * @return false          If segment is zero-length, or startD > stopD
   */
  bool GetSegment(float startD, float stopD, Path* dst, bool startWithMoveTo);

  /**
   *
   * @return true   If the current contour is Closed.
   */
  bool IsClosed();

  /**
   * @brief Move to next contour in path.
   *
   * @return true   If next contour exisits.
   * @return false  Reach the end.
   */
  bool NextContour();

 private:
  std::unique_ptr<ContourMeasureIter> iter_;
  std::shared_ptr<ContourMeasure> contour_;
};

}  // namespace skity

#endif  // INCLUDE_SKITY_GRAPHIC_PATH_MEASURE_HPP
