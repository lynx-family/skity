# Skity Xcframework

This directory contains the cmake script and other resources to build the skity xcframework.
Note:
1. The xcframework only contains static library. Since we do not provide code sign in this project.
2. The xcframework contains:
    - arm64 and x86_64 architecture for MacOS
    - arm64 architecture for iOS
    - x86_64 and arm64 architecture for iOS simulator
3. The xcframework does not provide module.modulemap, which means does not provide any API for swift project.

## Build Xcframework

We provide a python script to build the xcframework [pack_skity_framework.py](../../tools/pack_skity_framework.py)
Just run this script in project root directory.

```bash

python3 tools/pack_skity_framework.py

```

If everything goes well, you will get a xcframework file in `out` directory.
The xcframework file name is `skity.xcframework`.

