/*
 * Copyright 2018 Google Inc.
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#ifndef MODULE_IO_SRC_PICTURE_PRIV_HPP
#define MODULE_IO_SRC_PICTURE_PRIV_HPP

#include <cstdint>

namespace skity {

// This value should be aligned with the value in SkPicturePriv.h

// V35: Store SkRect (rather then width & height) in header
// V36: Remove (obsolete) alphatype from SkColorTable
// V37: Added shadow only option to SkDropShadowImageFilter (last version to
// record CLEAR) V38: Added PictureResolution option to SkPictureImageFilter
// V39: Added FilterLevel option to SkPictureImageFilter
// V40: Remove UniqueID serialization from SkImageFilter.
// V41: Added serialization of SkBitmapSource's filterQuality parameter
// V42: Added a bool to SkPictureShader serialization to indicate
// did-we-serialize-a-picture? V43: Added DRAW_IMAGE and DRAW_IMAGE_RECT opt
// codes to serialized data V44: Move annotations from paint to drawAnnotation
// V45: Add invNormRotation to SkLightingShader.
// V46: Add drawTextRSXform
// V47: Add occluder rect to SkBlurMaskFilter
// V48: Read and write extended SkTextBlobs.
// V49: Gradients serialized as SkColor4f + SkColorSpace
// V50: SkXfermode -> SkBlendMode
// V51: more SkXfermode -> SkBlendMode
// V52: Remove SkTextBlob::fRunCount
// V53: SaveLayerRec clip mask
// V54: ComposeShader can use a Mode or a Lerp
// V55: Drop blendmode[] from MergeImageFilter
// V56: Add TileMode in SkBlurImageFilter.
// V57: Sweep tiling info.
// V58: No more 2pt conical flipping.
// V59: No more LocalSpace option on PictureImageFilter
// V60: Remove flags in picture header
// V61: Change SkDrawPictureRec to take two colors rather than two alphas
// V62: Don't negate size of custom encoded images (don't write origin x,y
// either) V63: Store image bounds (including origin) instead of just
// width/height to support subsets V64: Remove occluder feature from blur
// maskFilter V65: Float4 paint color V66: Add saveBehind V67: Blobs serialize
// fonts instead of paints V68: Paint doesn't serialize font-related stuff V69:
// Clean up duplicated and redundant SkImageFilter related enums V70: Image
// filters definitions hidden, registered names updated to include "Impl" V71:
// Unify erode and dilate image filters V72: SkColorFilter_Matrix domain (rgba
// vs. hsla) V73: Use SkColor4f in per-edge AA quad API V74:
// MorphologyImageFilter internal radius is SkScaler V75: SkVertices switched
// from unsafe use of SkReader32 to SkReadBuffer (like everything else) V76: Add
// filtering enum to ImageShader V77: Explicit filtering options on imageshaders
// V78: Serialize skmipmap data for images that have it
// V79: Cubic Resampler option on imageshader
// V80: Smapling options on imageshader
// V81: sampling parameters on drawImage/drawImageRect/etc.
// V82: Add filter param to picture-shader
// V83: SkMatrixImageFilter now takes SkSamplingOptions instead of
// SkFilterQuality V84: SkImageFilters::Image now takes SkSamplingOptions
// instead of SkFilterQuality V85: Remove legacy support for inheriting sampling
// from the paint. V86: Remove support for custom data inside SkVertices V87:
// SkPaint now holds a user-defined blend function (SkBlender), no longer has
// DrawLooper V88: Add blender to ComposeShader and BlendImageFilter V89:
// Deprecated SkClipOps are no longer supported V90: Private API for backdrop
// scale factor in SaveLayerRec V91: Added raw image shaders V92: Added
// anisotropic filtering to SkSamplingOptions V94: Removed local matrices from
// SkShaderBase. Local matrices always use SkLocalMatrixShader. V95:
// SkImageFilters::Shader only saves SkShader, not a full SkPaint V96:
// SkImageFilters::Magnifier updated with more complete parameters V97:
// SkImageFilters::RuntimeShader takes a sample radius V98: Merged
// SkImageFilters::Blend and ::Arithmetic implementations V99: Remove legacy
// Magnifier filter V100: SkImageFilters::DropShadow does not have a dedicated
// implementation V101: Crop image filter supports all SkTileModes instead of
// just kDecal V102: Convolution image filter uses ::Crop to apply tile mode
// V103: Remove deprecated per-image filter crop rect
// v104: SaveLayer supports multiple image filters
// v105: Unclamped matrix color filter
// v106: SaveLayer supports custom backdrop tile modes
// v107: Combine SkColorShader and SkColorShader4
// v108: Serialize stable keys of runtime effects
// v109: Extend SkWorkingColorSpaceShader to have alpha type + output control

enum Version {
  kPictureShaderFilterParam_Version = 82,
  kMatrixImageFilterSampling_Version = 83,
  kImageFilterImageSampling_Version = 84,
  kNoFilterQualityShaders_Version = 85,
  kVerticesRemoveCustomData_Version = 86,
  kSkBlenderInSkPaint = 87,
  kBlenderInEffects = 88,
  kNoExpandingClipOps = 89,
  kBackdropScaleFactor = 90,
  kRawImageShaders = 91,
  kAnisotropicFilter = 92,
  kBlend4fColorFilter = 93,
  kNoShaderLocalMatrix = 94,
  kShaderImageFilterSerializeShader = 95,
  kRevampMagnifierFilter = 96,
  kRuntimeImageFilterSampleRadius = 97,
  kCombineBlendArithmeticFilters = 98,
  kRemoveLegacyMagnifierFilter = 99,
  kDropShadowImageFilterComposition = 100,
  kCropImageFilterSupportsTiling = 101,
  kConvolutionImageFilterTilingUpdate = 102,
  kRemoveDeprecatedCropRect = 103,
  kMultipleFiltersOnSaveLayer = 104,
  kUnclampedMatrixColorFilter = 105,
  kSaveLayerBackdropTileMode = 106,
  kCombineColorShaders = 107,
  kSerializeStableKeys = 108,
  kWorkingColorSpaceOutput = 109,

  // Only SKPs within the min/current picture version range (inclusive) can be
  // read.
  //
  // When updating kMin_Version also update oldestSupportedSkpVersion in
  // infra/bots/gen_tasks_logic/gen_tasks_logic.go
  //
  // Steps on how to find which oldestSupportedSkpVersion to use:
  // 1) Find the git hash when the desired kMin_Version was the kCurrent_Version
  // from the
  //    git logs:
  //    https://skia.googlesource.com/skia/+log/main/src/core/SkPicturePriv.h
  //    Eg:
  //    https://skia.googlesource.com/skia/+/bfd330d081952424a93d51715653e4d1314d4822%5E%21/#F1
  //
  // 2) Use that git hash to find the SKP asset version number at that time
  // here:
  //    https://skia.googlesource.com/skia/+/bfd330d081952424a93d51715653e4d1314d4822/infra/bots/assets/skp/VERSION
  //
  // 3) [Optional] Increment the SKP asset version number from step 3 and verify
  // that it has
  //    the expected version number by downloading the asset and running skpinfo
  //    on it.
  //
  // 4) Use the incremented SKP asset version number as the
  // oldestSupportedSkpVersion in
  //    infra/bots/gen_tasks_logic/gen_tasks_logic.go
  //
  // 5) Run `make -C infra/bots train`
  //
  // Contact the Infra Gardener if the above steps do not work for you.
  kMin_Version = kPictureShaderFilterParam_Version,
  kCurrent_Version = kWorkingColorSpaceOutput
};

// When we read/write the SkPictInfo via a stream, we have a sentinel byte right
// after the info. Note: in the read/write buffer versions, we have a slightly
// different convention:
//      We have a sentinel int32_t:
//          0 : failure
//          1 : PictureData
//         <0 : -size of the custom data
enum {
  kFailure_TrailingStreamByteAfterPictInfo = 0,      // nothing follows
  kPictureData_TrailingStreamByteAfterPictInfo = 1,  // SkPictureData follows
  kCustom_TrailingStreamByteAfterPictInfo = 2,       // -size32 follows
};

static constexpr uint32_t set_four_byte_tag(char a, char b, char c, char d) {
  return ((static_cast<uint32_t>(a) << 24) | (static_cast<uint32_t>(b) << 16) |
          (static_cast<uint32_t>(c) << 8) | (static_cast<uint32_t>(d)));
}

#define SK_PICT_READER_TAG set_four_byte_tag('r', 'e', 'a', 'd')
#define SK_PICT_FACTORY_TAG set_four_byte_tag('f', 'a', 'c', 't')
#define SK_PICT_TYPEFACE_TAG set_four_byte_tag('t', 'p', 'f', 'c')
#define SK_PICT_PICTURE_TAG set_four_byte_tag('p', 'c', 't', 'r')
#define SK_PICT_DRAWABLE_TAG set_four_byte_tag('d', 'r', 'a', 'w')

// This tag specifies the size of the ReadBuffer, needed for the following tags
#define SK_PICT_BUFFER_SIZE_TAG set_four_byte_tag('a', 'r', 'a', 'y')
// these are all inside the ARRAYS tag
#define SK_PICT_PAINT_BUFFER_TAG set_four_byte_tag('p', 'n', 't', ' ')
#define SK_PICT_PATH_BUFFER_TAG set_four_byte_tag('p', 't', 'h', ' ')
#define SK_PICT_TEXTBLOB_BUFFER_TAG set_four_byte_tag('b', 'l', 'o', 'b')
#define SK_PICT_SLUG_BUFFER_TAG set_four_byte_tag('s', 'l', 'u', 'g')
#define SK_PICT_VERTICES_BUFFER_TAG set_four_byte_tag('v', 'e', 'r', 't')
#define SK_PICT_IMAGE_BUFFER_TAG set_four_byte_tag('i', 'm', 'a', 'g')

// Always write this last (with no length field afterwards)
#define SK_PICT_EOF_TAG set_four_byte_tag('e', 'o', 'f', ' ')

enum SaveLayerRecFlatFlags {
  SAVELAYERREC_HAS_BOUNDS = 1 << 0,
  SAVELAYERREC_HAS_PAINT = 1 << 1,
  SAVELAYERREC_HAS_BACKDROP = 1 << 2,
  SAVELAYERREC_HAS_FLAGS = 1 << 3,
  SAVELAYERREC_HAS_CLIPMASK_OBSOLETE = 1 << 4,    // 6/13/2020
  SAVELAYERREC_HAS_CLIPMATRIX_OBSOLETE = 1 << 5,  // 6/13/2020
  SAVELAYERREC_HAS_BACKDROP_SCALE = 1 << 6,
  SAVELAYERREC_HAS_MULTIPLE_FILTERS = 1 << 7,
  SAVELAYERREC_HAS_BACKDROP_TILEMODE = 1 << 8,
};

enum DrawAtlasFlags {
  DRAW_ATLAS_HAS_COLORS = 1 << 0,
  DRAW_ATLAS_HAS_CULL = 1 << 1,
  DRAW_ATLAS_HAS_SAMPLING = 1 << 2,
};

enum SaveBehindFlatFlags {
  SAVEBEHIND_HAS_SUBSET = 1 << 0,
};

class ReadBuffer;

void SkipPictureInBuffer(ReadBuffer& buffer);

}  // namespace skity

#endif  // MODULE_IO_SRC_PICTURE_PRIV_HPP
