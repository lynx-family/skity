# Skity Codec: Decode-Time Downscaling

This document describes the design for decoding an image **directly at a
requested target size** (decode-time downscaling), instead of always decoding
at intrinsic size and letting the caller shrink the result afterwards.

> Status: **Implemented (Phase 0–3).** Phases 0 through 3 landed; Phase 4
> items remain future work. Notable deltas from the original proposal are
> recorded inline below and in §10.

## 1. Motivation & Goals

The codec module always decodes at the image's intrinsic size and outputs an
unpremul RGBA `Pixmap` (`include/skity/codec/codec.hpp`, `Codec::Decode()`).
For large images this allocates a full-size intermediate buffer even when the
consumer only needs a small render target:

| Image | Intrinsic RGBA buffer | Target 1080-wide RGBA buffer |
|---|---|---|
| 8000 × 6000 JPEG | ~183 MB | ~3.1 MB |
| 4096 × 4096 PNG | ~64 MB | ~4.5 MB |
| 3840 × 2160 WebP | ~33 MB | ~4.2 MB |

The cost is threefold: peak heap memory, decode CPU time, and the subsequent
texture upload / caching of oversized data by the consumer.

Some of the underlying decoders can produce reduced-resolution output natively
at a fraction of the full-decode cost. This design wires that capability
through the codec API, and provides a decode + resample fallback for formats
whose decoder cannot.

### Goals

- Add a way to request a target output size on `Codec::Decode()`.
- Use native decoder-side scaling where available (JPEG DCT scaling, WebP
  rescaler, ImageIO thumbnails) — saving both memory **and** CPU.
- Provide a shared high-quality fallback resampler for formats without native
  support (PNG, GIF, BMP), so the API contract is uniform.
- **Zero new third-party dependencies** (binary size is a core KPI; all
  required capability already exists in the linked libraries).
- Default behavior (no target size) is bit-identical to today.

### Non-goals

- Animated WebP / GIF scaled decoding with per-frame scaled compositing
  (`MultiFrameDecoder` keeps decoding frames at intrinsic size in v1).
- Region-of-interest (crop) decoding.
- PNG streaming row-sampling decode (peak-memory optimization for PNG;
  possible follow-up, see §8).
- EXIF orientation handling.
- Plumbing a target size from the rendering layer (image cache / draw-time
  scale selection) — the caller decides the target for now.

## 2. Current state

Audit of what each codec path uses today and what its underlying library
supports:

| Format | Library / current entry point | Native decode-time downscale? |
|---|---|---|
| JPEG | libjpeg-turbo, raw libjpeg API — `src/codec/jpeg_codec.cc` `Decode()` | **Yes.** `cinfo.scale_num / cinfo.scale_denom` set between `jpeg_read_header()` and `jpeg_start_decompress()` selects IDCT-scaled decoding. Discrete ratios n/8 (1/8 … 8/8); `output_width / output_height` become the scaled dims. Not currently used. |
| WebP (single frame) | libwebp, but via `WebPAnimDecoder` — `src/codec/webp/webp_decoder.cc` | **Yes, but not on the current path.** `WebPDecoderConfig.options.use_scaling / scaled_width / scaled_height` decodes to any target size via libwebp's built-in rescaler (optional sharp-YUV). `WebPAnimDecoderOptions` exposes no scaling — the anim decoder cannot scale. |
| WebP (animation) | same | **No** (via `WebPAnimDecoder`). Would require demux + per-frame scaled decode + manual blend/dispose at scaled coordinates. |
| PNG | libpng simplified API `png_image_begin_read_from_memory()` — `src/codec/png_codec.cc` | **No.** libpng has no scaled decoding, and the simplified API is whole-image; even row-skipping is unavailable without dropping to the low-level streaming API. |
| GIF | wuffs — `src/codec/wuffs/` | **No.** wuffs is a pure decoder; no resize. (GIF canvases are rarely the memory problem.) |
| BMP | in-house `src/codec/bmp_codec.cc` | **No**, but BMP is uncompressed — trivial to add sampling later. |
| Apple platforms (darwin framework build only) | ImageIO `CGImageSourceCreateImageAtIndex` — `src/codec/apple/codec_apple.mm` | **Yes, unused.** `CGImageSourceCreateThumbnailAtIndex` + `kCGImageSourceThumbnailMaxPixelSize` decodes to a requested max dimension natively (and honors EXIF orientation with `kCGImageSourceCreateThumbnailWithTransform`). |

### 2.1 Prior art: Skia

Skia decodes with the same underlying libraries and solves the same problem
in `SkCodec` (verified against `include/codec/SkCodec.h`,
`src/codec/SkJpegCodec.cpp`, `src/codec/SkWebpCodec.cpp`,
`src/codec/SkPngCodec.cpp` on `main`):

- **API model — query + negotiate, not exact-target.** Scaled decoding is
  driven by passing a *smaller* destination `SkImageInfo` to `getPixels()`:
  *"A size that does not match getInfo() implies a request to scale. If the
  generator cannot perform this scale, it will return kInvalidScale."*
  `getScaledDimensions(float)` reports the closest scale a codec natively
  supports; upscaling is refused (`>= 1.0` returns intrinsic size). The
  default `onGetScaledDimensions` returns intrinsic size — a per-codec
  best-effort contract, same as §3.
- **JPEG** — `SkJpegCodec::onGetScaledDimensions` negotiates over the fixed
  set {1/8, 1/4, 3/8, 1/2, 5/8, 3/4, 7/8, 1/1} (e.g.
  `desiredScale >= 0.9375 → 8/8`), querying output dims by setting
  `scale_num/scale_denom` on a temporary `dinfo` and calling
  `jpeg_calc_output_dimensions()`; the chosen ratio is applied to the real
  decompressor before `jpeg_start_decompress()`. §4.1 mirrors this,
  including the level table.
- **WebP** — `SkWebpCodec::onGetPixels` sets `config.options.use_scaling
  = 1` with `scaled_width/scaled_height` taken directly from the
  destination info; subset frames scale `dstX/dstY` by the same ratio
  (floored, conservatively). There is no `onGetScaledDimensions` override —
  libwebp scales to *any* size exactly, so no negotiation is needed. §4.2
  uses the same mechanism.
- **PNG** — libpng has no scaling; `SkPngCodec` uses the low-level
  row-callback API and drops unneeded rows
  (`swizzler()->rowNeeded(rowNum)`), sampling columns inside `SkSwizzler`
  (nearest-neighbor). Interlaced PNGs must be fully decoded into an
  interlace buffer before sampling. Decoder-side output is deliberately
  cheap/low-quality; high-quality downscaling happens at render time
  (`drawImageRect` + `SkSamplingOptions`, mipmaps).
- **Notable deltas vs this design:** Skia has no exact-target-size codec
  API (negotiation model only; our aspect-fit box is closer to Android
  `ImageDecoder` semantics), and Skia ships **no** decoder-side
  high-quality resampler — see §5 and §9 Q4 for the size implications.

## 3. API

```cpp
// include/skity/codec/codec.hpp

struct DecodeOptions {
  /**
   * Requested output size, in pixels. Semantics:
   *
   *  - Both 0 (default): decode at intrinsic size — identical to the
   *    current behavior.
   *  - One of them 0: derived from the other, preserving aspect ratio.
   *  - Both non-zero: aspect-fit box — the output is scaled to fit within
   *    (target_width x target_height) preserving aspect ratio (see §3.1).
   *
   * The codec never upscales: if the intrinsic size already fits within the
   * requested box, the image is decoded at intrinsic size.
   */
  int32_t target_width = 0;
  int32_t target_height = 0;

  /**
   * Quality hint for the fallback resampler / sharp-YUV path. Default keeps
   * the current trade-off; reserved for tuning, may be removed before the
   * API stabilizes.
   */
  bool prefer_quality = false;
};

class Codec {
  // Existing no-argument Decode() is kept; it forwards to
  // Decode(DecodeOptions{}).
  virtual std::shared_ptr<Pixmap> Decode() final;

  // New best-effort entry point. The returned Pixmap's Width()/Height() are
  // authoritative — callers must read the actual output size instead of
  // assuming the request was honored exactly (see §3.2).
  virtual std::shared_ptr<Pixmap> Decode(const DecodeOptions& options) = 0;
};
```

`Decode()` stays as a non-virtual final wrapper so every existing subclass and
caller compiles unchanged; each codec implements the new virtual method.
`MultiFrameDecoder::DecodeFrame()` is unchanged in v1 (frames decode at
intrinsic size); extending it is §8 follow-up work.

The stateful `SetData()` + `Decode()` pattern is preserved. This aligns with
the existing plan recorded in the header to eventually pass `Data` into
`Decode()` — `DecodeOptions` composes with that future signature as
`Decode(Data, DecodeOptions)`.

### 3.1 Why aspect-fit instead of exact stretch

A caller that knows its draw rectangle almost always wants the *longest side*
bounded, not a distorted stretch — this matches `kCGImageSourceThumbnailMaxPixelSize`,
Glide `override(w, h)`, and Coil semantics. An exact-stretch variant, if ever
needed, can be layered on aspect-fit later without breaking it.

### 3.2 Why best-effort instead of guaranteed exact output

JPEG's native scaling is limited to n/8 ratios; honoring an exact 0.37×
request would force the full-decode + resample path and give up the CPU win.
The contract is therefore: *output dimensions fit within the request,
preserve aspect ratio, and are as close to the request as the format's fast
path allows*. When the fast path lands more than a few percent away from the
requested size, the shared resampler closes the remaining gap (§5) — so in
practice outputs match the request unless the caller opted out.

## 4. Per-format strategy

### 4.1 JPEG — native DCT scaling (the big win)

In `JPEGCodec::Decode(options)`:

1. Compute the aspect-fit target `(tw, th)` from `options` and the header
   dims.
2. Choose the smallest supported ratio `r >= min(tw/w, th/h)` from the fixed
   set {1/8, 1/4, 3/8, 1/2, 5/8, 3/4, 7/8, 1/1} that still **covers** the
   target in both axes — the same level table `SkJpegCodec` negotiates over
   (§2.1); `scale_num/scale_denom` follow from the chosen level. Exact
   output dims come from `jpeg_calc_output_dimensions()` rather than
   hand-computed `ceil()`.
3. Set it after `jpeg_read_header()`, before `jpeg_start_decompress()`; the
   existing scanline loop, partial-decode `setjmp/longjmp` recovery, and
   pre-fill already key off `cinfo.output_width / output_scanline`, so they
   adapt without restructuring.
4. If the native dims are not exactly the target, run the shared resampler
   (§5) for the remainder. With the covering rule the native output is
   always >= the target, so this final pass only ever downsamples (up to
   ~1/7 of a step) — cheap and blur-free.

   Implementation note: the original "largest n/8 ≤ scale" (floor) rule was
   changed to the covering (ceil) rule after review — floor can undershoot
   the target by up to ~12.5%, which forced the remainder pass to
   **upscale**, discarding detail at IDCT time and blurring the grow step
   (e.g. 133×100 → target 66×50 picked 3/8 → 50×38 → grown back to 66×50).
   With ceil the native output covers the target, the remainder resample is
   always a downscale, and requests that land exactly on the n/8 grid still
   take the pure native path with no second pass (§9 Q2).

IDCT-scaled decoding is not an approximation hack: libjpeg computes a scaled
inverse DCT, which for downscale ratios is quality-comparable to (and much
faster than) full decode + post-resample, and it decodes strictly fewer
coefficients.

### 4.2 WebP — native rescaler (single-frame path only)

`WebpDecoder` gains an options-aware decode for the `frame_count == 1` case:

- Demux frame 1's payload (`WebPDemuxGetFrame` already runs in the
  constructor loop).
- Decode with `WebPDecoderConfig`: `options.use_scaling = 1`,
  `scaled_width/scaled_height` set to the aspect-fit target — libwebp hits
  the exact requested size, no second pass.
- `prefer_quality` maps to `options.use_sharp_yuv` (better chroma at some
  CPU cost).
- Animated WebP (`frame_count > 1`) keeps the `WebPAnimDecoder` canvas path
  at intrinsic size in v1; per-frame scaled compositing is §8. The canvas is
  then resampled to the target with the shared resampler (§5), so the output
  contract stays uniform across formats.

Concretely:

```c
WebPDecoderConfig config;
WebPInitDecoderConfig(&config);
config.output.colorspace = MODE_RGBA;
config.options.use_scaling = 1;
config.options.scaled_width  = target_width;   // exact, any ratio
config.options.scaled_height = target_height;
WebPDecode(frame_payload, frame_payload_size, &config);
```

This is the same mechanism `SkWebpCodec::onGetPixels` uses (§2.1) — no
approximation, no second pass; libwebp's internal rescaler hits the
requested dimensions exactly. `WebPDecode` above is the one-shot entry;
`WebPIDecode` accepts the same config for incremental decoding.

### 4.3 PNG / GIF / BMP — decode + shared resample

No native support. Decode exactly as today, then run the shared resampler
(§5) to the target. For PNG this trades memory honestly: peak is still the
intrinsic buffer, but the *returned* pixmap and everything downstream
(texture upload, caching) is target-sized. The streaming rework to also cut
PNG peak memory is deliberately deferred (§8).

### 4.4 Apple backend (darwin framework build)

`CodecApple::Decode(options)` switches to
`CGImageSourceCreateThumbnailAtIndex` with:

- `kCGImageSourceThumbnailMaxPixelSize = max(tw, th)` — ImageIO's own
  aspect-fit semantics match §3 exactly;
- `kCGImageSourceCreateThumbnailWithTransform = YES` for EXIF orientation;
- `kCGImageSourceShouldCacheImmediately = YES`.

For multi-frame GIF the per-frame `DecodeFrame` keeps
`CGImageSourceCreateImageAtIndex` (thumbnails of animation frames are §8).

## 5. Shared fallback resampler

One scaler, used by §4.1's remainder pass and all of §4.3:

- Input/output: unpremul RGBA `Pixmap` (the module's canonical output).
- **Correctness:** filter in premultiplied space, convert back to unpremul
  on output — filtering straight (unpremultiplied) RGB across varying alpha
  produces color bleed on semi-transparent edges.
- Area-average (box) resampling, separable two passes (horizontal then
  vertical), fixed-point weights; output pixel averages all contributing
  source pixels exactly, so integer ratios are clean subsampling and
  arbitrary ratios stay alias-free. Bilinear is rejected: it aliases badly
  at large downscale factors, which is precisely this feature's main case.
- Implementation home: `src/codec/codec_priv` alongside the existing line
  transform helpers. No dependency on core-skity drawing code; the codec
  module must stay self-contained.
- Size budget: a few hundred lines. The codec `.so` ships it regardless of
  which formats a consumer enables; measured size impact to be verified
  against the binary-size KPI before merge (§7, Phase 0 exit criterion).
  Escape hatch if the delta is unacceptable: degrade to pure box/nearest
  sampling and lean on render-time filtering for quality — exactly what
  Skia does (§2.1, §9 Q4).

## 6. Behavior matrix

| Request | JPEG | WebP (1 frame) | PNG / GIF / BMP | Apple backend |
|---|---|---|---|---|
| No target (default) | intrinsic (unchanged) | intrinsic (unchanged) | intrinsic (unchanged) | intrinsic (unchanged) |
| Target ≤ intrinsic | native n/8 + optional small resample | native, exact target | intrinsic + resample | ImageIO thumbnail, exact |
| Target ≥ intrinsic | intrinsic, no upscale | intrinsic, no upscale | intrinsic, no upscale | intrinsic, no upscale |
| Truncated / corrupt (JPEG) | partial result at *scaled* size (existing longjmp path) | n/a | n/a | n/a |

## 7. Rollout

- **Phase 0 — plumbing: done.** `DecodeOptions`, new virtual
  `Decode(options)`, no-arg `Decode()` wrapper, shared resampler + unit
  tests (`test/ut/codec/codec_scale_test.cc`). Measured
  `libskity-codec.dylib` (Release, macOS/arm64): 1,495,208 → 1,497,064
  bytes, **+1,856 B (+0.12%)** — accepted against the size KPI.
- **Phase 1 — JPEG native scaling: done.**
- **Phase 2 — WebP single-frame native scaling: done.**
- **Phase 3 — fallback resample for PNG / GIF / BMP + Apple thumbnail path:
  done.**
- **Phase 4 (stretch, §8):** scaled animated decode, PNG streaming decode.

Testing notes:

- Unit tests per format in `test/ut/codec/`: dimension assertions (aspect-fit
  math, no-upscale clamp, one-dimension-unspecified), and content checks
  against a reference (full decode + independent scaler) with a PSNR /
  max-delta tolerance rather than exact pixels.
- JPEG quality regression guard (`JPEGDecodeScaledHighFrequencyQuality`):
  noise content scaled just below an n/8 grid point; asserts the output
  keeps ≥ 0.62 of the reference's row-difference energy. The floor rule
  measured ~0.53 (decode-undersized + interpolated growth), the covering
  rule ~0.71.
- WebP: a legal single-frame animation (`single_frame_anim.webp`, canvas
  200×200, frame 128×128 at an offset) must composite onto the canvas —
  `frame_count == 1` alone does not identify a static WebP
  (`ANIMATION_FLAG` does).
- JPEG: truncated-file partial decode at scaled size must still return a
  partial pixmap (existing behavior, new dims).
- Golden tests are unaffected by construction — they call the no-argument
  path.

## 8. Future work

- Scaled animated decoding (WebP animation via demux + per-frame scaled
  decode + scaled-rect blend/dispose; GIF analogous over wuffs).
- PNG low-level streaming decode + row sampling to cut *peak* memory for
  huge PNGs (interlaced PNGs excluded).
- Target-size plumbing from the rendering/image-cache layer so callers get
  decode-time downscaling without computing sizes themselves.
- `MultiFrameDecoder::DecodeFrame(frame, prev, options)`.

## 9. Open questions

1. Aspect-fit box vs exact-stretch semantics — aspect-fit proposed (§3.1);
   confirm with the primary consumer (Lynx image pipeline).
2. ~~Whether the JPEG remainder pass threshold (~2%) should instead always
   resample to exact target~~ — resolved (and tightened after review): the
   level rule is "smallest n/8 that covers the target" and the remainder
   pass resamples to the exact target whenever the native output misses it,
   so the pass only ever downsamples (§4.1).
3. Whether `prefer_quality` survives into the stable API or remains
   internal. Note: libwebp's decoder has no sharp-YUV option (see §10), so
   the flag currently has no effect on any backend.
4. ~~Decoder-side quality vs binary size~~ — resolved: the area-average
   resampler ships; measured delta +1,856 B (§7).

## 10. Implementation notes ( deltas from the original proposal)

- **`WebPInitDecoderConfig`, not `WebPDecoderConfigInit`** — the proposal's
  snippet used a wrong symbol name; the actual libwebp API is
  `WebPInitDecoderConfig()` (decode.h).
- **No `use_sharp_yuv` on the decode path** — `WebPDecoderOptions` has no
  such field (sharp YUV is an encoder-side concept in libwebp), so
  `prefer_quality` is currently inert for WebP.
- **Animated WebP fallback resamples** — the anim canvas decodes at
  intrinsic size (as proposed) and then goes through the shared resampler,
  keeping the output contract uniform across formats.
- **`ResolveTargetSize` is header-inline in `codec_priv.hpp`** — the darwin
  framework build compiles only `codec_apple.mm` without `codec_priv.cc`,
  and the ImageIO path needs the same aspect-fit negotiation without
  linking the resampler.
- **JPEG level rule is "smallest n/8 that covers the target"** (§4.1,
  revised after review) — the original proposal's "largest n/8 ≤ scale"
  (floor) rule could undershoot the target by up to ~12.5% and forced an
  upscaling remainder pass (decode → shrink → blur-grow). The covering rule
  keeps every remainder resample a pure downscale; the ~2% threshold was
  dropped in favor of "resample whenever the native output is not exactly
  the target" for fully predictable output sizes.
