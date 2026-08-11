package kench1994.github.io

import android.Manifest
import android.app.NativeActivity
import android.content.Context
import android.content.pm.PackageManager
import android.location.Location
import android.location.LocationListener
import android.location.LocationManager
import android.os.Build
import android.os.Bundle
import android.view.KeyEvent
import android.view.View
import android.view.WindowInsets
import android.view.WindowInsetsController
import android.view.inputmethod.InputMethodManager
import java.util.concurrent.LinkedBlockingQueue

class MainActivity : NativeActivity() {
    public override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        hideSystemUI()
        // NativeActivity 可能在创建 Surface 后重置窗口标志，监听“系统栏重新可见”并立即再隐藏
        if (Build.VERSION.SDK_INT < Build.VERSION_CODES.R) {
            @Suppress("DEPRECATION")
            window.decorView.setOnSystemUiVisibilityChangeListener { visibility ->
                if ((visibility and View.SYSTEM_UI_FLAG_FULLSCREEN) == 0) hideSystemUI()
            }
        }
        // 注意：不要在 onCreate 里 post 调用 nativeSetSafeAreaInsets —— RegisterNatives 在 native
        // 主循环 android_main 中执行，晚于本 onCreate，post 会早于注册导致 UnsatisfiedLinkError 崩溃。
        // 安全区改由 onWindowFocusChanged（时机远晚于注册）统一回传。
    }

    // 焦点变化（如切回 App）后系统栏可能复现，重新隐藏并刷新安全区
    override fun onWindowFocusChanged(hasFocus: Boolean) {
        super.onWindowFocusChanged(hasFocus)
        if (hasFocus) {
            hideSystemUI()
            pushSafeAreaInsets()
        }
    }

    // 读取顶部刘海/挖孔安全区，回传给 native 供 ImGui 顶部避让；不处理四角圆角
    private fun pushSafeAreaInsets() {
        val insets = window.decorView.rootWindowInsets ?: return
        var top = 0; var right = 0; var bottom = 0; var left = 0
        // 仅取顶部刘海/挖孔高度（API 29+）；左右/底部始终 0
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.Q) {
            top = insets.displayCutout?.safeInsetTop ?: 0
        }
        try {
            nativeSetSafeAreaInsets(top.toFloat(), right.toFloat(), bottom.toFloat(), left.toFloat())
        } catch (e: UnsatisfiedLinkError) {
            // native 尚未完成 JNI 注册（极早期调用）时忽略，安全区暂留 0；下次 onWindowFocusChanged 会补传
        }
    }

    // 沉浸式全屏：隐藏状态栏与导航栏，消除程序周边“边框”
    private fun hideSystemUI() {
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.R) {
            window.setDecorFitsSystemWindows(false)
            window.insetsController?.let { c ->
                c.hide(WindowInsets.Type.statusBars() or WindowInsets.Type.navigationBars())
                c.systemBarsBehavior = WindowInsetsController.BEHAVIOR_SHOW_TRANSIENT_BARS_BY_SWIPE
            }
        } else {
            @Suppress("DEPRECATION")
            window.decorView.systemUiVisibility = (View.SYSTEM_UI_FLAG_IMMERSIVE_STICKY
                or View.SYSTEM_UI_FLAG_FULLSCREEN
                or View.SYSTEM_UI_FLAG_HIDE_NAVIGATION
                or View.SYSTEM_UI_FLAG_LAYOUT_STABLE
                or View.SYSTEM_UI_FLAG_LAYOUT_HIDE_NAVIGATION
                or View.SYSTEM_UI_FLAG_LAYOUT_FULLSCREEN)
        }
    }

    fun showSoftInput() {
        val inputMethodManager = getSystemService(Context.INPUT_METHOD_SERVICE) as InputMethodManager
        inputMethodManager.showSoftInput(this.window.decorView, 0)
    }

    fun hideSoftInput() {
        val inputMethodManager = getSystemService(Context.INPUT_METHOD_SERVICE) as InputMethodManager
        inputMethodManager.hideSoftInputFromWindow(this.window.decorView.windowToken, 0)
    }

    // Queue for the Unicode characters to be polled from native code (via pollUnicodeChar())
    private var unicodeCharacterQueue: LinkedBlockingQueue<Int> = LinkedBlockingQueue()

    // We assume dispatchKeyEvent() of the NativeActivity is actually called for every
    // KeyEvent and not consumed by any View before it reaches here
    override fun dispatchKeyEvent(event: KeyEvent): Boolean {
        if (event.action == KeyEvent.ACTION_DOWN) {
            unicodeCharacterQueue.offer(event.getUnicodeChar(event.metaState))
        }
        return super.dispatchKeyEvent(event)
    }

    fun pollUnicodeChar(): Int {
        return unicodeCharacterQueue.poll() ?: 0
    }

    // ---- 定位相关 ----
    // 由 C++ 经 RegisterNatives 绑定的 native 回调
    external fun nativeOnLocation(lat: Double, lon: Double, alt: Double, acc: Double, ts: Long)
    external fun nativeOnPermissionResult(granted: Boolean)
    external fun nativeOnHeading(headingRadians: Double)

    // 安全区（圆角/挖孔）内边距，由 Kotlin 读出后回传 native 供 ImGui 收敛进安全矩形
    external fun nativeSetSafeAreaInsets(top: Float, right: Float, bottom: Float, left: Float)

    private val LOCATION_REQ = 1001
    private var locationManager: LocationManager? = null

    private val locationListener = object : LocationListener {
        override fun onLocationChanged(loc: Location) {
            nativeOnLocation(loc.latitude, loc.longitude, loc.altitude, loc.accuracy.toDouble(), loc.time)
        }
        override fun onStatusChanged(provider: String?, status: Int, extras: Bundle?) {}
        override fun onProviderEnabled(provider: String) {}
        override fun onProviderDisabled(provider: String) {}
    }

    fun requestLocationPermission() {
        // JNI 从 native 线程调入，权限请求须切到 UI 线程执行
        runOnUiThread {
            if (checkSelfPermission(Manifest.permission.ACCESS_FINE_LOCATION) == PackageManager.PERMISSION_GRANTED) {
                // 已授权则直接回调，避免重复弹窗
                nativeOnPermissionResult(true)
            } else {
                requestPermissions(arrayOf(Manifest.permission.ACCESS_FINE_LOCATION), LOCATION_REQ)
            }
        }
    }

    override fun onRequestPermissionsResult(requestCode: Int, permissions: Array<out String>, grantResults: IntArray) {
        super.onRequestPermissionsResult(requestCode, permissions, grantResults)
        if (requestCode == LOCATION_REQ) {
            val granted = grantResults.isNotEmpty() && grantResults[0] == PackageManager.PERMISSION_GRANTED
            nativeOnPermissionResult(granted)
        }
    }

    fun startLocationUpdates() {
        // JNI 从 native 线程调入；requestLocationUpdates 的回调需要 Looper，
        // 切到 UI 线程注册并显式指定 mainLooper，避免 "Can't create handler" 崩溃
        runOnUiThread {
            if (checkSelfPermission(Manifest.permission.ACCESS_FINE_LOCATION) != PackageManager.PERMISSION_GRANTED)
                return@runOnUiThread
            val lm = locationManager
                ?: (getSystemService(Context.LOCATION_SERVICE) as LocationManager).also { locationManager = it }
            try {
                lm.requestLocationUpdates(LocationManager.GPS_PROVIDER, 1000L, 1f, locationListener, mainLooper)
            } catch (e: SecurityException) {
            }
        }
    }

    fun stopLocationUpdates() {
        runOnUiThread {
            locationManager?.removeUpdates(locationListener)
        }
    }
}
