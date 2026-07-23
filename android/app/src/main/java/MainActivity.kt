package kench1994.github.io

import android.Manifest
import android.app.NativeActivity
import android.content.Context
import android.content.pm.PackageManager
import android.location.Location
import android.location.LocationListener
import android.location.LocationManager
import android.os.Bundle
import android.view.KeyEvent
import android.view.inputmethod.InputMethodManager
import java.util.concurrent.LinkedBlockingQueue

class MainActivity : NativeActivity() {
    public override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
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
        requestPermissions(arrayOf(Manifest.permission.ACCESS_FINE_LOCATION), LOCATION_REQ)
    }

    override fun onRequestPermissionsResult(requestCode: Int, permissions: Array<out String>, grantResults: IntArray) {
        super.onRequestPermissionsResult(requestCode, permissions, grantResults)
        if (requestCode == LOCATION_REQ) {
            val granted = grantResults.isNotEmpty() && grantResults[0] == PackageManager.PERMISSION_GRANTED
            nativeOnPermissionResult(granted)
        }
    }

    fun startLocationUpdates() {
        if (checkSelfPermission(Manifest.permission.ACCESS_FINE_LOCATION) != PackageManager.PERMISSION_GRANTED)
            return
        val lm = locationManager
            ?: (getSystemService(Context.LOCATION_SERVICE) as LocationManager).also { locationManager = it }
        try {
            lm.requestLocationUpdates(LocationManager.GPS_PROVIDER, 1000L, 1f, locationListener)
        } catch (e: SecurityException) {
        }
    }

    fun stopLocationUpdates() {
        locationManager?.removeUpdates(locationListener)
    }
}
