## 项目说明
- 当前项目使用c++、xmake构建，Imgui为UI框架，metal为macos上的渲染引擎，DX11为win上的渲染引擎，android上使用OpenGL3
- **阶段一范围**：macOS + Android
- **macOS**：CoreLocation 的 `CLLocation.altitude` 本身就是 MSL，无需修正
- **Android**：`Location.getAltitude()` 返回 WGS84 椭球高，需用 GeographicLib（EGM96 geoid 模型）修正为 MSL
- **阶段二（本次不做）**：提升精度（融合气压计、EGM2008 等）

## 项目结构
- `Backend/Backend.{h,cpp}`：单例 `Backend`
- `Frontend/Frontend.{h,cpp}`：单例 `Frontend`，`update()` 每帧绘制；当前主窗口是空壳（`// TODO：绘制主界面`）。`init()` 在各平台入口被调用。
- 入口：
  - macOS `mainMacDesktop.mm`：Cocoa 应用，`drawInMTKView` 每帧调 `Frontend::Instance().update()`，主线程跑 `NSApp run`。
  - Android `mainAndroid.cpp`：`native_app_glue`，`android_main` 循环调 `MainLoopStep()`→`Frontend::update()`。已有 JNI 调 `MainActivity.kt` 的先例（`showSoftInput` / `pollUnicodeChar`），通过 `g_App->activity->vm` / `->clazz`。
- `assets/app/Info.plist`：macOS 打包用的 plist，当前**无定位权限声明**。xmake `after_build` 将其拷到 `.app/Contents/Info.plist`。
- Android 工程：基于 `ThirdParty/imgui/examples/example_android_opengl3/android/` 模板（`MainActivity.kt` 继承 `NativeActivity`、`AndroidManifest.xml`）。实际 `android/` 目录不在仓库内（外部维护），但模板揭示了接入方式。
- `utils/Logger.{h,cpp}`：spdlog 封装，可直接 `LOG_INFO` 等。
- 构建：xmake，已用 `add_requires("nlohmann_json v3.12.0")`；macOS 编 `*.mm` + osx/metal 后端，Android 编 android/opengl3 后端 + `mainAndroid.cpp`。

## 构建方法

1. **macOS 构建运行**：`xmake f -m releasedbg && xmake`，运行 `xmake run`：
2. **Android 构建运行**：`xmake f -p android -a arm64-v8a && xmake`，安装到真机：