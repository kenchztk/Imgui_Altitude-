# 气压海拔项目 — 长期记忆

## 设计稿状态
- Ardot 设计文件 `海拔气压 App`：四屏完整（实时/记录/地图/设置）
- 实时屏含模式切换器：海拔/气压/温度/趋势
- 设置屏完整映射 CLAUDE.md 配置：单位体系、自动校准、EGM96 修正、毛玻璃背景、主题切换
- Token 系统：App 变量集（accent=#2F6E5E 绿青色，单色语义化风格）
- border token 为 FLOAT 类型异常，统一用 #E6E8EC hex 替代

## UI 偏好（已确认）
- 无冗余顶部大标题，核心数字为视觉主角
- 单色语义化元素，拒绝装饰性渐变/彩虹配色
- 移动端 pill/segmented 控件带文字标签，非纯图标
- 字体：Sarasa Gothic SC

## 技术架构要点（来自 CLAUDE.md）
- macOS: Metal + CoreLocation (MSL altitude)
- Android: OpenGL ES 3 + LocationManager + EGM96 球谐系数修正
- 单例分层: Backend → Frontend → StyleManager
- 构建必须用 `open pkg/NativeApp.app`（非 xmake run）

## ImGui 四屏实现现状（2026-08-04）
- 已用 ImGui 落地四屏，复用深色主题 + 青蓝 accent(80,180,255)
- `Frontend::update` 编排：header(每屏标题) → body 子窗口(派发各屏) → 底部 NavBar pill
- 屏幕模块：`RealtimeScreen`(指标切换 海拔/气压/温度/趋势)、`RecordsScreen`(模拟数据)、`MapScreen`(坐标卡用真实定位)、`SettingsScreen`
- `AppSettings` 单例承载单位(米/英尺、hPa/mmHg、°C/°F)与开关，单位即时影响显示
- `LocationData` 无温度字段：实时屏温度按海拔直减率合成演示值
- 主题保持深色（用户选择保留当前深色，未套用设计稿浅色）
- Android 原"相框+景深"装饰（灰框/墙渐变/暗角/投影/顶部微光）已移除：根窗口改用系统安全区 inset（`AndroidGetSafeInsets`，Kotlin 经 JNI 回传）避让状态栏/挖孔，无边框/渐变/暗角；clear_color 已对齐画布色
