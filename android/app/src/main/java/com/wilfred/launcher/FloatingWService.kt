package com.wilfred.launcher

import android.app.Notification
import android.app.NotificationChannel
import android.app.NotificationManager
import android.app.PendingIntent
import android.app.Service
import android.content.Intent
import android.graphics.PixelFormat
import android.os.Build
import android.os.Handler
import android.os.IBinder
import android.os.Looper
import android.provider.Settings
import android.text.Editable
import android.text.TextWatcher
import android.view.Gravity
import android.view.LayoutInflater
import android.view.MotionEvent
import android.view.View
import android.view.WindowManager
import android.view.inputmethod.InputMethodManager
import android.widget.EditText
import android.widget.ImageButton
import android.widget.LinearLayout
import android.widget.TextView
import androidx.core.app.NotificationCompat
import androidx.recyclerview.widget.LinearLayoutManager
import androidx.recyclerview.widget.RecyclerView
import java.util.concurrent.Executors

/**
 * Floating "W" button pinned to the **bottom-left** corner of the screen.
 * Tapping it pops up the search bar (overlay popup with results) without
 * leaving the current app — the Android replacement for the desktop global
 * hotkey (Ctrl+Alt+W), which has no Android equivalent.
 *
 * All search logic lives in C++ (wilfred_core via WilfredBridge); this
 * service only owns the overlay windows and Android Intents.
 */
class FloatingWService : Service() {
    private var wm: WindowManager? = null
    private var button: ImageButton? = null
    private var popup: View? = null
    private var popupInput: EditText? = null
    private var popupAdapter: ResultAdapter? = null
    private var lastQuery: String = ""
    private var lastResults: List<SearchResult> = emptyList()
    private var searchSeq: Int = 0

    private val io = Executors.newSingleThreadExecutor()
    private val main = Handler(Looper.getMainLooper())

    override fun onBind(intent: Intent?): IBinder? = null

    override fun onCreate() {
        super.onCreate()
        startForegroundNotification()
        if (!Settings.canDrawOverlays(this)) {
            stopSelf()
            return
        }
        // Core may not be booted yet if the service starts before the
        // activity; boot it here so the popup works standalone.
        io.execute {
            if (!WilfredBridge.ready) {
                WilfredBridge.init(filesDir.absolutePath)
            }
        }
        showButton()
    }

    override fun onStartCommand(intent: Intent?, flags: Int, startId: Int): Int {
        if (button == null && Settings.canDrawOverlays(this)) showButton()
        if (intent?.action == ACTION_TOGGLE_SEARCH) togglePopup()
        return START_STICKY
    }

    override fun onDestroy() {
        removePopup()
        removeButton()
        io.shutdownNow()
        super.onDestroy()
    }

    // ---- Foreground notification (required for overlay services) ----

    private fun startForegroundNotification() {
        val channelId = "wilfred_overlay"
        if (Build.VERSION.SDK_INT >= 26) {
            val nm = getSystemService(NotificationManager::class.java)
            nm?.createNotificationChannel(
                NotificationChannel(channelId, "Wilfred", NotificationManager.IMPORTANCE_MIN)
            )
        }
        val open = PendingIntent.getActivity(
            this, 0, Intent(this, MainActivity::class.java),
            PendingIntent.FLAG_UPDATE_CURRENT or PendingIntent.FLAG_IMMUTABLE
        )
        val n: Notification = NotificationCompat.Builder(this, channelId)
            .setContentTitle("Wilfred")
            .setContentText("Tap the W button to search")
            .setSmallIcon(R.drawable.ic_w)
            .setContentIntent(open)
            .setOngoing(true)
            .build()
        startForeground(1, n)
    }

    // ---- Floating W button (bottom-left) ----

    private fun showButton() {
        if (button != null) return
        val manager = getSystemService(WINDOW_SERVICE) as WindowManager
        wm = manager
        val btn = ImageButton(this).apply {
            setImageResource(R.drawable.ic_w)
            contentDescription = getString(R.string.floating_w_desc)
            background = null
            // Slight elevation shadow on supported versions.
            if (Build.VERSION.SDK_INT >= 21) elevation = 8f
        }
        // Tap vs drag: short tap toggles the search popup; drag repositions
        // the button (clamped to bottom-left half on release).
        var downX = 0f
        var downY = 0f
        var startX = 0
        var startY = 0
        var dragging = false
        var paramsHolder: WindowManager.LayoutParams? = null
        btn.setOnTouchListener { v, ev ->
            val params = paramsHolder
            when (ev.action) {
                MotionEvent.ACTION_DOWN -> {
                    downX = ev.rawX
                    downY = ev.rawY
                    if (params != null) {
                        startX = params.x
                        startY = params.y
                    }
                    dragging = false
                    false
                }
                MotionEvent.ACTION_MOVE -> {
                    val dx = (ev.rawX - downX).toInt()
                    val dy = (ev.rawY - downY).toInt()
                    if (!dragging && (dx * dx + dy * dy) > 144) dragging = true
                    if (dragging && params != null) {
                        params.x = startX + dx
                        params.y = startY + dy
                        try { wm?.updateViewLayout(v, params) } catch (_: Exception) { }
                    }
                    dragging
                }
                MotionEvent.ACTION_UP -> {
                    if (!dragging) v.performClick()
                    else {
                        // Snap back to the bottom-left corner region.
                        params?.let {
                            it.gravity = Gravity.START or Gravity.BOTTOM
                            try { wm?.updateViewLayout(v, it) } catch (_: Exception) { }
                        }
                    }
                    true
                }
                else -> false
            }
        }
        btn.setOnClickListener { togglePopup() }
        val type = if (Build.VERSION.SDK_INT >= 26) {
            WindowManager.LayoutParams.TYPE_APPLICATION_OVERLAY
        } else {
            @Suppress("DEPRECATION")
            WindowManager.LayoutParams.TYPE_PHONE
        }
        val size = resources.getDimensionPixelSize(R.dimen.floating_w_size)
        val params = WindowManager.LayoutParams(
            size,
            size,
            type,
            WindowManager.LayoutParams.FLAG_NOT_FOCUSABLE,
            PixelFormat.TRANSLUCENT
        ).apply {
            // Bottom-left corner per spec.
            gravity = Gravity.START or Gravity.BOTTOM
            x = resources.getDimensionPixelSize(R.dimen.floating_w_margin)
            y = resources.getDimensionPixelSize(R.dimen.floating_w_bottom_margin)
        }
        paramsHolder = params
        manager.addView(btn, params)
        button = btn
    }

    private fun removeButton() {
        try {
            button?.let { wm?.removeView(it) }
        } catch (_: Exception) {
        }
        button = null
        wm = null
    }

    // ---- Search-bar popup ----

    private fun togglePopup() {
        if (popup != null) {
            removePopup()
        } else {
            showPopup()
        }
    }

    private fun showPopup() {
        if (popup != null) return
        val manager = getSystemService(WINDOW_SERVICE) as WindowManager
        val view = LayoutInflater.from(this).inflate(R.layout.popup_search, null)
        val input = view.findViewById<EditText>(R.id.popup_input)
        val list = view.findViewById<RecyclerView>(R.id.popup_list)
        val status = view.findViewById<TextView>(R.id.popup_status)
        val correction = view.findViewById<TextView>(R.id.popup_correction)
        val close = view.findViewById<View>(R.id.popup_close)

        val adapter = ResultAdapter(
            onClick = { r, idx ->
                WilfredActions.openResult(this, r, lastQuery, idx) { runPopupSearch(input, status, correction) }
            },
            onLongClick = { r, idx ->
                WilfredActions.showActionsDialog(this, r, idx) { runPopupSearch(input, status, correction) }
            }
        )
        popupAdapter = adapter
        list.layoutManager = LinearLayoutManager(this)
        list.adapter = adapter

        val type = if (Build.VERSION.SDK_INT >= 26) {
            WindowManager.LayoutParams.TYPE_APPLICATION_OVERLAY
        } else {
            @Suppress("DEPRECATION")
            WindowManager.LayoutParams.TYPE_PHONE
        }
        val params = WindowManager.LayoutParams(
            WindowManager.LayoutParams.MATCH_PARENT,
            WindowManager.LayoutParams.WRAP_CONTENT,
            type,
            WindowManager.LayoutParams.FLAG_NOT_TOUCH_MODAL or
                WindowManager.LayoutParams.FLAG_WATCH_OUTSIDE_TOUCH,
            PixelFormat.TRANSLUCENT
        ).apply {
            gravity = Gravity.START or Gravity.BOTTOM
            y = resources.getDimensionPixelSize(R.dimen.floating_w_size) +
                resources.getDimensionPixelSize(R.dimen.floating_w_bottom_margin) + 8
        }
        // Cap height: at most ~65% of the screen.
        view.findViewById<LinearLayout>(R.id.popup_card)?.let {
            it.layoutParams?.height
        }
        try {
            manager.addView(view, params)
        } catch (_: Exception) {
            return
        }
        popup = view
        popupInput = input

        close.setOnClickListener { removePopup() }
        correction.setOnClickListener {
            val fix = correction.tag as? String
            if (!fix.isNullOrEmpty()) {
                input.setText(fix)
                input.setSelection(fix.length)
            }
        }
        input.addTextChangedListener(object : TextWatcher {
            override fun beforeTextChanged(s: CharSequence?, a: Int, b: Int, c: Int) = Unit
            override fun onTextChanged(s: CharSequence?, a: Int, b: Int, c: Int) = Unit
            override fun afterTextChanged(s: Editable?) {
                runPopupSearch(input, status, correction)
            }
        })
        input.setOnEditorActionListener { _, _, _ ->
            val top = lastResults.firstOrNull()
            if (top != null) WilfredActions.openResult(this, top, lastQuery, 0) {
                runPopupSearch(input, status, correction)
            }
            true
        }
        // Boot core if needed, then initial search.
        io.execute {
            if (!WilfredBridge.ready) WilfredBridge.init(filesDir.absolutePath)
            val clip = WilfredActions.androidClipboardText(this)
            if (clip.isNotEmpty()) WilfredBridge.pushClipboard(clip)
            main.post {
                status.text = getString(R.string.search_hint)
                runPopupSearch(input, status, correction)
            }
        }
        input.requestFocus()
        try {
            val imm = getSystemService(INPUT_METHOD_SERVICE) as InputMethodManager
            input.postDelayed({ imm.showSoftInput(input, InputMethodManager.SHOW_IMPLICIT) }, 150)
        } catch (_: Exception) { }
    }

    private fun runPopupSearch(input: EditText, status: TextView, correction: TextView) {
        val query = input.text.toString()
        lastQuery = query
        if (!WilfredBridge.ready) {
            status.text = getString(R.string.status_boot_failed)
            return
        }
        val seq = ++searchSeq
        io.execute {
            val results = WilfredBridge.search(query, 30)
            val assist = WilfredBridge.assist(query)
            main.post {
                if (seq != searchSeq) return@post
                if (popup == null) return@post
                if (query != input.text.toString()) return@post
                lastResults = results
                popupAdapter?.submit(results)
                status.text = if (query.isBlank()) getString(R.string.search_hint)
                else if (results.isEmpty()) getString(R.string.no_results)
                else "${results.size} results"
                if (assist.correction.isNotEmpty() && query.isNotBlank()) {
                    correction.visibility = View.VISIBLE
                    correction.tag = assist.correction
                    correction.text = getString(R.string.did_you_mean, assist.correction)
                } else {
                    correction.visibility = View.GONE
                }
            }
        }
    }

    private fun removePopup() {
        try {
            popup?.let { wm?.removeView(it) }
        } catch (_: Exception) {
        }
        popup = null
        popupInput = null
        popupAdapter = null
    }

    companion object {
        const val ACTION_TOGGLE_SEARCH = "com.wilfred.launcher.TOGGLE_SEARCH"

        fun startIfPermitted(context: android.content.Context) {
            if (!Settings.canDrawOverlays(context)) return
            val i = Intent(context, FloatingWService::class.java)
            if (Build.VERSION.SDK_INT >= 26) context.startForegroundService(i)
            else context.startService(i)
        }
    }
}
