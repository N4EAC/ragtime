# Ragtime for Android

A Kotlin and Compose app over Ragtime's C++ engines. What it does, its
four views and how it is put together are in
[docs/ANDROID.md](../docs/ANDROID.md); this page is for building and
testing it.

The app does not bundle, start or replace the modem. Run Data2G
separately and give Settings its address, your callsign and the KISS and
command ports (session data uses the command port + 1). To try the app
with no modem at all, turn on **Demo mode** in Settings.

## Build

The toolchain: JDK 17, Gradle 8.9 (the wrapper fetches it), Android
Gradle Plugin 8.7.2, Kotlin 2.0.0, Compose BOM 2024.10.01, Android SDK
36 (compile and target), NDK 28.2.13676358 and the SDK's CMake 3.22.1.
The minimum is Android 8 (API 26), and the APK holds ARM64 only.

Install the SDK components and accept their licences through Android's
SDK manager. Set `JAVA_HOME` to JDK 17 and `ANDROID_HOME` to your SDK
(or create an ignored `local.properties` with `sdk.dir=...`). Then:

```sh
cd android
./gradlew :app:assembleDebug :app:lintDebug
```

The APK is
`app/build/outputs/apk/debug/ragtime-android-<version>-b<build>-debug.apk`,
named from `versionName` and `versionCode` in `app/build.gradle.kts`.
Raise the build number with every APK handed to someone. Open `android/`
in Android Studio to run it.

The Gradle project builds the core, the picture codecs and the adapter
into `libragtime_jni.so` with its own CMake
(`app/src/main/cpp/CMakeLists.txt`), with the desktop app, audio,
Hamlib and the other optional libraries off. For pictures the first
configure clones OpenJPEG 2.5.3 from GitHub
(`cmake/FetchOpenJpeg.cmake`), so that build needs the network once.

The build copies the repository's `LICENSE`, the Cool-chic notices,
`docs/ANDROID_PRIVACY.md` and `docs/ANDROID_RELEASE_NOTES.md` into the
APK, so the app cannot drift from them: rebuild after changing one. The
Apache License is the asset `app/src/main/assets/licenses/apache-2.0.txt`.

The device tests pin Espresso 3.7.0: the older version the Compose test
library brings in fails on Android 17 (API 37)
([AndroidX Test release notes](https://developer.android.com/jetpack/androidx/releases/test#espresso-3.7.0)).

## Testing

There are three kinds of test.

**The adapter's**, with no phone. They are built with the desktop tests
on Linux and the Mac and run under CTest (`ctest -R android`):

- `ragtime_android_client_test`: fragmented command input, the Hello
  masks, UTF-8 in the JSON snapshot, opening a session, and broadcast
  numbering kept across a restart.
- `ragtime_android_files_test`: two adapters on fake hosts through
  session and broadcast files, checked byte for byte, with rows kept
  across a restart and paths outside the station's folder refused.
- `ragtime_android_gear_test`: the gearshift from the host's thresholds.
- `ragtime_android_pictures_test`: a picture coded within its bytes,
  shown from any first part, and sent in a ragchew and on D2G.
- `ragtime_android_demo_test`: demo mode's whole net with an adapter on
  it.

**The JVM's**, for the demo guide's steps and for what is worth a
notification:

```sh
cd android
./gradlew :app:testDebugUnitTest
```

**The device's**, on a phone or an ARM64 emulator on adb, against the
fake-host bench. Never point them at a modem that is on a radio. Start
`ragtime_fake_air` from a desktop build, with its ports free:

```sh
build/tests/ragtime_fake_air --bots 0 --airtime 0
```

Then name the device:

```sh
tools/android-smoke.sh emulator-5554
tools/android-smoke.sh PHONE_SERIAL 10000
```

The script builds and installs both debug APKs on only the named device,
forwards the bench's ports to it, runs the tests, removes the forwards
and puts the app's saved settings back. The second argument moves the
bench's ports on the device (18100, 18300 and so on): use it on a phone
whose own Data2G TNC holds 8100 and 8300, so no test meets a real modem.
The script grants the notification permission, which Connect would
otherwise ask for over the screen the tests drive. A new emulator needs
4 GB of memory (`hw.ramSize`) and a few idle minutes after its first
boot, or Android kills the app for starting too slowly.

The device tests cover the JNI library as packaged (broadcasts with
UTF-8 and emoji, acknowledgements, a ragchew, files each way, Leave,
saved history, reconnect), documents in and out, the four views and
their drafts, the ragchew kept by the service while the screen is
stopped, pictures, the gearshift, the roll call, the notices and
notifications. Demo mode's, the notices' and the notifications' tests
need no bench.

Before distributing a build, also check by hand what the tests cannot
reach: Android's own document and photo pickers and Save a copy, the
notification's Disconnect, the screen off and the app killed while
connected, a link dropped in the middle of a file, and a session with
a real modem. Automated tests cannot establish that a line gets through
on the air.
