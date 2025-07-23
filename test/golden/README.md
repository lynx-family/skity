
## Skity Golden Test

Golden test is a test case that compare the result of the test case with the golden image.
It is used to test the correctness of the Skity rendering result.

### Compile and Run

Since the golden test code use Metal backend to do offline rendering, you should compile the golden test code with Metal backend.
Also the golden test code use codec module to load the golden image, so you should compile the golden test code with codec module.

To compile the golden test code, you should pass `-DSKITY_MTL_BACKEND=ON` and `-DSKITY_CODEC_MODULE=ON` to cmake.

> Note:
> Currently the golden test code is only supported on macOS.

```bash
cmake -B <build directory> -DSKITY_MTL_BACKEND=ON -DSKITY_TEST=ON -DSKITY_CODEC_MODULE=ON ...
```

By default, the `SKITY_GOLDEN_GUI` option is enabled, which will show a Window and the compare result.
The window provides the following features:
- `s` to save the current result to the golden image.
- close the window will run the next test case or exit the test program.

> Note:
> We are planning to add more features and GUI widgets to the golden test window.

If you want to disable the GUI, you can pass `-DSKITY_GOLDEN_GUI=OFF` to cmake.

After compile, you can run the golden test code by the following command:

```bash
./<build directory>/test/golden/skity_golden_test
```

Since we use [GoogleTest](https://github.com/google/googletest) to test the golden test code,
you can use the `--gtest_filter` option to run the specific test case.

```bash
./<build directory>/test/golden/skity_golden_test --gtest_filter=<test_case_name>

# Run all GradientGolden test cases
./<build directory>/test/golden/skity_golden_test --gtest_filter="GradientGolden*"

# Only run the GradientGolden.LinearGradientTileMode test case
./<build directory>/test/golden/skity_golden_test --gtest_filter=GradientGolden.LinearGradientTileMode
```


### Add Golden Test Case
To add a golden test case, you should add a subdirectory to the `test/golden/cases` directory.
Add a line to the `test/golden/cases/CMakeLists.txt` file to add the subdirectory to the test case list.

```cmake
add_subdirectory(<test_case_name>)
```

The subdirectory should contain the following files:
- CMakeLists.txt the CMakeLists.txt file for the test case
- source files the source files for the test case
- golden image the golden image for the test case

#### CMakeLists.txt
We provide a cmake function `add_golden_test` to help add the test code.

```cmake

add_golden_test(
  NAME <test_case_name>
  SOURCES <test_case_source_files>
  INCLUDES <test_case_include_directories>
  LIBS <test_case_link_libraries>
  DEFINES <test_case_compile_definitions>
)

```


> Note:
> The `add_golden_test` function will automatically define `CASE_DIR` to the directory of the test case.
> But you can also define some other macros to the test case.

You can reference existing test cases to see how to use the function.

#### Test Case Source Files

We use [GoogleTest](https://github.com/google/googletest) to test the golden test code.
Also we provide a function `skity::testing::CompareGoldenTexture` to compare the golden image with the test result.

A simple test case looks like this:
```cpp
#include <gtest/gtest.h>
#include <skity/recorder/picture_recorder.hpp>
#include "common/golden_test_check.hpp"

// the macro CASE_DIR point to the directory of the test case defined by the cmake function add_golden_test
constexpr const char* kSourceDir = CASE_DIR; 

TEST(TestName, CaseName) {
  // create a display list
  skity::PictureRecorder recorder;
  auto canvas = recorder.beginRecording(100, 100);

  // draw something
  canvas->drawRect(skity::Rect::MakeXYWH(0, 0, 100, 100), skity::Paint());
  // end the display list
  auto picture = recorder.finishRecordingAsPicture();

  // compare the golden image with the test result
  EXPECT_TRUE(skity::testing::CompareGoldenTexture(picture, <width>, <height>, kSourceDir"/<case_name>.png"));
}
```

We recommend test one feature per test case. and make the golden image as small as possible.
This can make the test code more readable and maintainable. Also it can make the test code more efficient.

