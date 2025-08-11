# Skity Android Package

This directory contains a minimal gradle project to pack skity into Android AAR with [prefab](https://google.github.io/prefab/) enabled.

Which makes the other user can use cmake [find_package](https://cmake.org/cmake/help/latest/command/find_package.html) to link skity library packed inside the AAR.

To use this package, you need to add the following code to your `build.gradle` file:

```groovy
...
android {
    ...


    buildFeatures {
        prefab true # enable prefab features, this makes cmake script can find perfab module packed inside AAR.
    }
}

...

dependencies {
    implementation files('<path to local libs>/skity-native.aar')
}

```

In CMakeLists.txt，skity can be linked with the following code:

```cmake
find_package(skity REQUIRED CONFIG)


target_link_libraries(
    <your target>
    PUBLIC # or PRIVATE it is up to you
    skity::skity
)

```

## Build

* requirement:
    * Android SDK installed with API LEVEL 35
    * Android NDK installed with version 21.3.6528147
    * Java 17 or higher installed

Set up environment `ANDROID_HOME=<path to android sdk installed directory>`
Or create a `local.properties` file in the root directory of the project with the following content:
```
sdk.dir=<path to android sdk installed directory>
```

In this directory run:
```bash
./gradlew :assembleDebug # or ./gradlew :assembleRelease if want release build
```

If every thing goes well, you can find the AAR file in `./build/outputs/aar/` directory.
The AAR file name is `skity-native-debug.aar` or `skity-native-release.aar` depends on the build type.
