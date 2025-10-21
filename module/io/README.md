# Skity IO Module

This module provides IO functions for Skity to support read and write SKP files used in [SKIA Debugger Tools](https://skia.org/docs/dev/tools/debugger/). This allow Skity to record and play back draw commands, which is useful for debugging and testing.
> Note: The IO module is still under development and not stable. The API is subject to change in the future.

Current status:
* Support SKP version <= 109 (The version is defined in [SkPicturePriv.h](https://github.com/google/skia/blob/main/src/core/SkPicturePriv.h))
* Does not support recursive picture recording. If a picture contains a sub-picture, the reading process will stoped and return failed.
* Does not support reading picture with SLUG text rendering commands. Since Skity does not support it and does not know how to skip it in binary data.
