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

add_requires("nlohmann_json v3.12.0", { alias = "nlohmann_json", system = false})
add_requires("geographiclib 2.1.1",   { alias = "geographiclib", system = false, configs = {shared = false}})

target("NativeApp")
    if not is_plat("android") then
        set_kind("binary")
    end

    -- utils
    add_includedirs("utils")
    add_files("utils/*.cpp")
    -- packages
    add_packages("nlohmann_json", "geographiclib")
    -- 3rds
    add_includedirs(".", "ThirdParty")
    -- imgui
    add_files("ThirdParty/imgui/*.cpp", "*.cc")
    for _,f in ipairs({"imgui.cpp", "imgui_demo.cpp", "imgui_draw.cpp", "imgui_tables.cpp", "imgui_widgets.cpp"}) do
        add_files("ThirdParty/imgui/" .. f)
    end
    add_files("ThirdParty/imgui/*.cpp", "Frontend/*.cpp|WebViewPanel.cpp", "Backend/*.cpp")
    add_includedirs("ThirdParty/imgui", "ThirdParty/imgui/backends")
    -- IconFontCppHeaders
    add_includedirs("ThirdParty/IconFontCppHeaders")
    -- spdlog
    add_defines("SPDLOG_USE_STD_FORMAT", "SPDLOG_COMPILED_LIB")
    add_includedirs("ThirdParty/spdlog/include")
    add_files("ThirdParty/spdlog/src/*.cpp")

    if is_plat("windows") then
        add_defines("WIN32", "_WIN32")
        -- add_files("mainWinDesktop.cpp")
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
        add_files("*.mm")
        for _,f in ipairs({"osx", "metal"}) do
            add_files("ThirdParty/imgui/backends/imgui_impl_" .. f .. ".mm")
        end
        add_cxflags("-Wall", "-Wextra")
        add_mxflags("-fno-objc-arc")
        add_frameworks("AppKit", "Metal", "MetalKit", "QuartzCore", "GameController")
    elseif is_plat("android") then
        set_kind("shared")
        set_arch("arm64-v8a")
        add_defines("__ANDROID__")
        for _,f in ipairs({"android", "opengl3"}) do
            add_files("ThirdParty/imgui/backends/imgui_impl_" .. f .. ".cpp")
        end
        add_includedirs("$(ndk)/sources/android/native_app_glue")
        add_files("$(ndk)/sources/android/native_app_glue/android_native_app_glue.c");
        add_files("mainAndroid.cpp")
        -- JNI 头文件路径
        -- /System/Volumes/Data/Users/kench/Library/Android/sdk/ndk/27.0.12077973/sources/android/native_app_glue/android_native_app_glue.c
        -- add_includedirs("$(ndk)/27.0.12077973/sources/android/native_app_glue")
        add_defines("IMGUI_IMPL_OPENGL_ES3")
        add_ldflags("-u ANativeActivity_onCreate")
        add_ldflags("-Wl,--no-undefined", "-Wl,--exclude-libs,ALL", "-Wl,-Bsymbolic")
        add_syslinks("android", "EGL",  "GLESv3", "log")
    end


    after_build(function (target)
        -- 自动生成 compile_commands.json 帮助代码补全跳转
        import("core.base.task")
        task.run("project", {kind = "compile_commands", outputdir = ".vscode"})

        if target:is_plat("windows") then
            -- Windows：运行时依赖 WebView2Loader.dll，构建后拷贝到可执行文件输出目录
            local dest = path.directory(target:targetfile())
            os.cp("ThirdParty/WebView2/bin/x64/WebView2Loader.dll", dest)
            print("[WebView2] copied WebView2Loader.dll -> " .. dest)
       
        elseif is_plat("android") then
            local libname = path.filename(target:targetfile())
            local dest = path.join("android/app/libs/arm64-v8a", libname)
            os.cp(target:targetfile(), dest)
            print("✅ 已拷贝到 Android 项目: " .. dest)
        
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
            -- 设置可执行权限
            os.exec("chmod 755 " .. path.join(macos_dir, target:name()))
            print("✅ 已打包应用到: " .. app_path)
        end
    end)
target_end()

