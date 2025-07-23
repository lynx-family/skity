# Skiity Codec

The Codec module is a private module used inside Skity project to handle basic image decode and encode.
Currently it only support PNG and JPEG format. And it is not stable.

> **Warning**
>
> This module is not stable. Do not use this module in commercial project, otherwise you need to take your own risk.

This module depends on [libpng](https://github.com/pnggroup/libpng) and [libjpeg-turbo](https://github.com/libjpeg-turbo/libjpeg-turbo).
To fetch these should sync with `dev` target:

```
<path_to_habitat>/hab sync --target dev
```
