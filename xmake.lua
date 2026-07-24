set_languages("clatest", "cxx20")

add_rules("mode.releasedbg", "mode.debug", "mode.release")
set_allowedmodes("releasedbg", "debug", "release")
set_defaultmode("releasedbg")

set_exceptions("cxx")
set_encodings("utf-8")
add_cxflags("-fvisibility=hidden")
add_rpathdirs(".")

if not is_mode("release") then
    set_optimize("none")
    set_strip("none")
end
add_defines("ImTextureID=ImU64", "_UNICODE", "UNICODE", "__STDC_CONSTANT_MACROS", "__STDC_FORMAT_MACROS")

if is_plat("android") then
    set_config("ndk", "/System/Volumes/Data/Users/kench/Library/Android/sdk/ndk/27.0.12077973")
    set_config("ndk_sdkver", "27")
    set_config("ndk_cxxstl", "c++_shared")
    set_runtimes("c++_shared")
else
    set_runtimes("MD")
end

add_requires("fmt 12.2.0", { alias = "fmt", system = false, configs = {header_only = true}})
add_requires("nlohmann_json v3.12.0", { alias = "nlohmann_json", system = false})
add_requires("geographiclib 2.1.1",   { alias = "geographiclib", system = false, configs = {shared = false}})

-- ============================================================================
-- imgui 动态库（先 macOS 跑通，后续扩展 Android / Windows）
-- 把 Dear ImGui 核心 + 平台渲染后端编成共享库 libimgui.{dylib,so,dll}，
-- 宿主(NativeApp)通过 add_deps 链接，运行时随平台包分发。
-- macOS 用 @loader_path 作为 install_name，dylib 与可执行文件同目录即可解析。
-- ============================================================================
if is_plat("macosx", "android") then
    target("imgui")
        set_kind("shared")
        add_includedirs("ThirdParty/imgui", "ThirdParty/imgui/backends")
        -- ImGui 核心（所有 *.cpp，不含 backends 子目录）
        add_files("ThirdParty/imgui/*.cpp")
        if is_plat("macosx") then
            -- macOS 后端：OSX(Cocoa) + Metal
            for _,f in ipairs({"osx", "metal"}) do
                add_files("ThirdParty/imgui/backends/imgui_impl_" .. f .. ".mm")
            end
            add_defines('IMGUI_API=__attribute__((visibility("default")))')
            add_cxflags("-Wall", "-Wextra")
            add_mxflags("-fno-objc-arc")
            add_frameworks("AppKit", "Metal", "MetalKit", "QuartzCore", "GameController", "Foundation")
            add_ldflags("-install_name @loader_path/libimgui.dylib")
        else -- android
            -- Android 后端：Android(NativeActivity) + OpenGL ES3
            for _,f in ipairs({"android", "opengl3"}) do
                add_files("ThirdParty/imgui/backends/imgui_impl_" .. f .. ".cpp")
            end
            -- 全局 -fvisibility=hidden 下必须用 default 可见性暴露 IMGUI_API
            add_defines('IMGUI_API=__attribute__((visibility("default")))')
            add_defines("IMGUI_IMPL_OPENGL_ES3")
            add_syslinks("GLESv3", "EGL", "android", "log")
            set_arch("arm64-v8a")
        end
    target_end()
end

target("NativeApp")
    if not is_plat("android") then
        set_kind("binary")
    end

    -- utils
    add_includedirs("utils")
    add_files("utils/*.cpp")
    -- packages
    add_packages("geographiclib", "nlohmann_json", "fmt")
    -- 3rds
    add_includedirs(".", "ThirdParty")
    -- imgui
    -- macOS 走共享库 libimgui（见上方独立 target）；其余平台直接编进 NativeApp
    if not is_plat("macosx", "android") then
        add_files("ThirdParty/imgui/*.cpp")
        for _,f in ipairs({"imgui.cpp", "imgui_demo.cpp", "imgui_draw.cpp", "imgui_tables.cpp", "imgui_widgets.cpp"}) do
            add_files("ThirdParty/imgui/" .. f)
        end
    end
    add_files("Frontend/*.cpp|WebViewPanel.cpp", "Backend/*.cpp")
    add_includedirs("ThirdParty/imgui", "ThirdParty/imgui/backends")
    if is_plat("macosx", "android") then
        add_deps("imgui")
    end
    -- IconFontCppHeaders
    add_includedirs("ThirdParty/IconFontCppHeaders")
    -- spdlog
    add_defines("SPDLOG_FMT_EXTERNAL", "FMT_HEADER_ONLY=1", "SPDLOG_COMPILED_LIB")
    add_includedirs("ThirdParty/spdlog/include")
    add_files("ThirdParty/spdlog/src/*.cpp")

    if is_plat("windows") then
        add_defines("WIN32", "_WIN32")
        -- add_files("platform/win/Main.cpp")
        -- for _,f in ipairs({"win32", "dx11"}) do
        --     add_files("ThirdParty/imgui/backends/imgui_impl_" .. f .. ".cpp")
        -- end
        -- add_files("Frontend/WebViewPanel.cpp")
        -- add_syslinks("d3d11", "d3dcompiler", "dxgi", "Advapi32", "Shell32", "ole32", "User32")
        -- -- WebView2 SDK（头文件 + 导入库），用于 WebViewPanel.cpp 的原生浏览器封装
        -- add_includedirs("ThirdParty/WebView2/include")
        -- add_linkdirs("ThirdParty/WebView2/lib/x64")
        -- add_links("WebView2Loader.dll")
    elseif is_plat("macosx") then
        add_files("platform/mac/Main.mm")
        add_files("Backend/*.mm")
        add_cxflags("-Wall", "-Wextra")
        add_mxflags("-fno-objc-arc")
        add_frameworks("AppKit", "Metal", "MetalKit", "QuartzCore", "GameController", "CoreLocation", "CoreMotion", "CoreFoundation")
    elseif is_plat("android") then
        set_kind("shared")
        set_arch("arm64-v8a")
        add_defines("__ANDROID__")
        -- ImGui 后端(imgui_impl_android/opengl3)已编入共享库 libimgui.so，此处不再编译
        add_includedirs("$(ndk)/sources/android/native_app_glue")
        add_files("$(ndk)/sources/android/native_app_glue/android_native_app_glue.c");
        add_files("platform/android/Main.cpp")
        add_defines("IMGUI_IMPL_OPENGL_ES3")
        add_ldflags("-u ANativeActivity_onCreate")
        add_ldflags("-Wl,--no-undefined", "-Wl,--exclude-libs,ALL", "-Wl,-Bsymbolic")
        add_syslinks("android", "EGL",  "GLESv3", "log")
    end


    after_build(function (target)
        -- 自动生成 compile_commands.json 帮助代码补全跳转
        import("core.base.task")
        task.run("project", {kind = "compile_commands", outputdir = ".vscode"})

        local font_src = "assets/fonts/MapleMono-NF-CN-Regular.ttf"

        if target:is_plat("windows") then
            -- Windows：运行时依赖 WebView2Loader.dll，构建后拷贝到可执行文件输出目录
            local dest = path.directory(target:targetfile())
            os.cp("ThirdParty/WebView2/bin/x64/WebView2Loader.dll", dest)
            print("[WebView2] copied WebView2Loader.dll -> " .. dest)
            -- 字体分发：拷贝到可执行文件旁 fonts/，运行时以相对路径加载
            local font_dir = path.join(dest, "fonts")
            os.mkdir(font_dir)
            os.cp(font_src, font_dir)
            print("✅ 已拷贝字体: " .. path.join(font_dir, path.filename(font_src)))

        elseif is_plat("android") then
            -- xmake f -p android -a arm64-v8a --ndk=/System/Volumes/Data/Users/kench/Library/Android/sdk/ndk/27.0.12077973 --ndk_sdkver=27
            local libname = path.filename(target:targetfile())
            local dest = path.join("android/app/libs/arm64-v8a", libname)
            os.cp(target:targetfile(), dest)
            print("✅ 已拷贝到 Android 项目: " .. dest)
            -- 拷贝 libimgui.so 到同一 jniLibs 目录：由 libNativeApp.so 的 DT_NEEDED 依赖，
            -- 安装后由 Android 动态链接器自动先于 NativeApp 加载，无需在 Java 侧显式 loadLibrary
            local imgui_so = path.join(path.directory(target:targetfile()), "libimgui.so")
            if os.isfile(imgui_so) then
                os.cp(imgui_so, path.join("android/app/libs/arm64-v8a", "libimgui.so"))
                print("✅ 已拷贝 libimgui.so -> android/app/libs/arm64-v8a")
            else
                print("⚠️ 未找到 " .. imgui_so .. "，跳过 libimgui.so 拷贝")
            end
            -- 拷贝 c++_shared 运行时（NDK 自带），APK 安装后 NativeApp/libimgui 均依赖它
            local cxx_so = "$(ndk)/toolchains/llvm/prebuilt/darwin-x86_64/sysroot/usr/lib/aarch64-linux-android/libc++_shared.so"
            if os.isfile(cxx_so) then
                os.cp(cxx_so, path.join("android/app/libs/arm64-v8a", "libc++_shared.so"))
                print("✅ 已拷贝 libc++_shared.so -> android/app/libs/arm64-v8a")
            else
                print("⚠️ 未找到 libc++_shared.so（" .. cxx_so .. "），请手动补入 jniLibs")
            end
            -- 拷贝 EGM96 geoid 数据到 assets，供运行时 AAssetManager 读取
            local geoid_src = "assets/geoid/egm96-5.pgm"
            if os.isfile(geoid_src) then
                local geoid_dest_dir = "android/app/src/main/assets/geoid"
                os.mkdir(geoid_dest_dir)
                os.cp(geoid_src, path.join(geoid_dest_dir, "egm96-5.pgm"))
                print("✅ 已拷贝 EGM96 数据: " .. path.join(geoid_dest_dir, "egm96-5.pgm"))
            else
                print("⚠️ 未找到 " .. geoid_src .. "，跳过 EGM96 数据拷贝")
            end
            -- 字体分发：拷贝到 APK assets/fonts/，运行时经 AAssetManager 读取
            local font_dest_dir = "android/app/src/main/assets/fonts"
            os.mkdir(font_dest_dir)
            os.cp(font_src, font_dest_dir)
            print("✅ 已拷贝字体: " .. path.join(font_dest_dir, path.filename(font_src)))
            
        elseif is_plat("macosx") then
            local pkg_dir = "pkg"
            local app_name = target:name() .. ".app"
            local app_path = path.join(pkg_dir, app_name)
            local contents_dir = path.join(app_path, "Contents")
            local macos_dir = path.join(contents_dir, "MacOS")
            local resources_dir = path.join(contents_dir, "Resources")

            -- 创建标准 .app 目录结构
            os.mkdir(macos_dir)
            os.mkdir(resources_dir)

            -- 复制可执行文件
            os.cp(target:targetfile(), path.join(macos_dir, target:name()))
            -- 复制 Info.plist
            os.cp("assets/app/Info.plist", path.join(contents_dir, "Info.plist"))
            -- 字体分发：拷贝到 Resources/fonts/（须在 codesign 之前，随 bundle 一并签名）
            local font_dest_dir = path.join(resources_dir, "fonts")
            os.mkdir(font_dest_dir)
            os.cp(font_src, font_dest_dir)
            print("✅ 已拷贝字体: " .. path.join(font_dest_dir, path.filename(font_src)))
            -- 设置可执行权限
            os.exec("chmod 755 " .. path.join(macos_dir, target:name()))
            -- 拷贝 libimgui.dylib 到 MacOS 目录（install_name 用 @loader_path，与可执行文件同目录即可解析）
            local imgui_lib = path.join(path.directory(target:targetfile()), "libimgui.dylib")
            if os.isfile(imgui_lib) then
                os.cp(imgui_lib, macos_dir)
                print("✅ 已拷贝 libimgui.dylib -> " .. macos_dir)
            else
                print("⚠️ 未找到 " .. imgui_lib .. "，跳过 libimgui.dylib 拷贝")
            end
            -- 对整个 .app bundle 重新签名（ad-hoc）：
            -- 链接器自带的 ad-hoc 签名不会绑定 Info.plist，导致 CoreLocation 等依赖
            -- usage description 的隐私 API 静默失效（授权弹窗不弹）。重新签名让
            -- Info.plist 与资源被正确封印进签名，TCC 方可识别 app 身份与权限描述。
            os.exec("codesign --force --deep --sign - " .. app_path)
            print("✅ 已打包并签名应用到: " .. app_path)
        end
    end)
target_end()

