# GPU color glyphs and the path threshold

`Paint::SetFontThreshold` selects outline rendering for larger text. A
bitmap-only intrinsic-color glyph has no outline to substitute. Crossing that
threshold must preserve its bitmap instead of silently omitting the glyph.

Affine draws retain the established direct atlas geometry, sampling, intrinsic
color and paint-alpha behavior when the complete color-glyph group fits. If a
nonempty glyph cannot be represented by that atlas, discard the attempted
direct group before creating draw objects and use the existing bounded
transformed-mask fitting path for the whole group. Empty glyphs remain empty.
Perspective draws retain their existing transformed-mask path. Atlas limits,
threshold values and public interfaces are unchanged.

The permanent `ColorGlyphThresholdTest` Metal fixture compares color and alpha
pixels on both sides of the threshold, including fill, stroke, gradient paint,
Gaussian filtering and surface densities of one and two. A separate oversized
run exercises bounded bitmap fitting. The tests use actual texture submission,
completion and readback; their completion wait belongs only to the fixture.
They require macOS, CoreText, Apple Color Emoji and an available Metal device.

Run them through the unified runner:

```sh
python3 tools/test-runner.py --suite=unit --backend=metal \
  --filter='ColorGlyphThresholdTest.*'
```
