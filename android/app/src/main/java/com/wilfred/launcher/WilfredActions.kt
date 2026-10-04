package com.wilfred.launcher

import android.app.AlertDialog
import android.content.ClipData
import android.content.ClipboardManager
import android.content.Context
import android.content.Intent
import android.content.pm.PackageManager
import android.media.AudioManager
import android.net.Uri
import android.provider.Settings
import android.widget.Toast
import androidx.core.content.FileProvider
import org.json.JSONObject
import java.io.File

/**
 * Shared result-opening logic for MainActivity and the floating popup.
 * All ranking/search/calc/snippets/notes/todos/timers logic lives in C++
 * (wilfred_core via JNI); Kotlin only performs Android Intents, clipboard
 * writes and file sharing.
 */
object WilfredActions {

    fun copyText(context: Context, text: String, toast: Boolean = true) {
        if (text.isEmpty()) return
        try {
            val cm = context.getSystemService(Context.CLIPBOARD_SERVICE) as ClipboardManager
            cm.setPrimaryClip(ClipData.newPlainText("Wilfred", text))
            WilfredBridge.pushClipboard(text)
            if (toast) Toast.makeText(context, context.getString(R.string.copied), Toast.LENGTH_SHORT).show()
        } catch (_: Exception) { }
    }

    fun androidClipboardText(context: Context): String {
        return try {
            val cm = context.getSystemService(Context.CLIPBOARD_SERVICE) as ClipboardManager
            val clip = cm.primaryClip ?: return ""
            if (clip.itemCount == 0) return ""
            clip.getItemAt(0)?.coerceToText(context)?.toString().orEmpty()
        } catch (_: Exception) { "" }
    }

    /** Tap (primary) action. [index] is the position in the last search results. */
    fun openResult(context: Context, r: SearchResult, query: String, index: Int, onRefresh: (() -> Unit)? = null) {
        WilfredBridge.recordChoice(query, if (r.path.isNotEmpty()) r.path else r.payload)
        // Toggles / settings / config / approvals are handled natively
        // (system intents, AudioManager, brightness) instead of the C++
        // desktop backends, which are stubs on Android.
        if (handleMobileSystem(context, r, index, onRefresh)) return
        try {
            // App packages enumerated from PackageManager.
            if (r.path.startsWith("package:")) {
                val pkg = r.path.removePrefix("package:")
                val intent = context.packageManager.getLaunchIntentForPackage(pkg)
                if (intent != null) {
                    intent.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK)
                    context.startActivity(intent)
                } else {
                    Toast.makeText(context, pkg, Toast.LENGTH_SHORT).show()
                }
                return
            }
            // Calculator / converter / generic copy cards: copy payload.
            if (r.action == "calc" || r.action == "convert" || r.action == "copy" ||
                r.action == "mini" && copyableMini(r)
            ) {
                // Give C++ a chance to run side effects (timer_stop etc. are
                // separate menu actions; primary is copy).
                WilfredBridge.execute(index, "copy_text")
                copyText(context, if (r.payload.isNotEmpty()) r.payload else r.title)
                onRefresh?.invoke()
                return
            }
            // Snippet expand: paste = copy body.
            if (r.action == "expand" || r.category == "snippet") {
                WilfredBridge.execute(index, "paste")
                copyText(context, if (r.payload.isNotEmpty()) r.payload else r.title)
                return
            }
            // Web results.
            if (r.action == "web" || r.path.startsWith("http://") || r.path.startsWith("https://") ||
                r.payload.startsWith("http://") || r.payload.startsWith("https://")
            ) {
                val url = if (r.path.startsWith("http")) r.path else r.payload
                if (url.isNotEmpty()) {
                    context.startActivity(Intent(Intent.ACTION_VIEW, Uri.parse(url)).apply {
                        addFlags(Intent.FLAG_ACTIVITY_NEW_TASK)
                    })
                } else {
                    copyText(context, r.title)
                }
                return
            }
            // Desktop-only cards: report clearly instead of failing silently.
            if (r.action == "window" || r.category == "window" ||
                r.action == "system" || r.category == "system" ||
                r.action == "screenshot" || r.category == "screenshot"
            ) {
                val msg = when {
                    r.action == "window" || r.category == "window" -> context.getString(R.string.not_supported_window)
                    r.action == "system" || r.category == "system" -> context.getString(R.string.not_supported_system)
                    else -> context.getString(R.string.not_supported_screenshot)
                }
                Toast.makeText(context, msg, Toast.LENGTH_SHORT).show()
                return
            }
            // Files / folders via FileProvider.
            if (r.path.isNotEmpty()) {
                val f = File(r.path)
                if (f.exists()) {
                    val uri = FileProvider.getUriForFile(context, "${context.packageName}.provider", f)
                    val intent = Intent(Intent.ACTION_VIEW, uri).apply {
                        addFlags(Intent.FLAG_GRANT_READ_URI_PERMISSION)
                        addFlags(Intent.FLAG_ACTIVITY_NEW_TASK)
                    }
                    val chooser = Intent.createChooser(intent, r.title).apply {
                        addFlags(Intent.FLAG_ACTIVITY_NEW_TASK)
                    }
                    context.startActivity(chooser)
                    return
                }
            }
            // Fallback: copy whatever text the card carries.
            val text = when {
                r.payload.isNotEmpty() -> r.payload
                r.path.isNotEmpty() -> r.path
                else -> r.title
            }
            copyText(context, text)
            onRefresh?.invoke()
        } catch (e: Exception) {
            Toast.makeText(context, e.message ?: "Cannot open", Toast.LENGTH_SHORT).show()
        }
    }

    /**
     * Native Android handling for toggle/settings/config/setup/plugin cards.
     * Returns true when the card was handled (caller returns immediately).
     * Wi-Fi/Bluetooth open system settings (apps cannot toggle radios since
     * Android 10); volume uses AudioManager for real; brightness writes
     * SCREEN_BRIGHTNESS when WRITE_SETTINGS is granted, else opens display
     * settings; config/setup/plugin payloads run through C++ (which edits the
     * mobile wilfred.yml / trust store under the app files dir).
     */
    private fun handleMobileSystem(context: Context, r: SearchResult, index: Int, onRefresh: (() -> Unit)?): Boolean {
        val payload = if (r.payload.isNotEmpty()) r.payload else r.path
        if (r.category == "toggle" || payload.startsWith("toggle:")) {
            when {
                payload.startsWith("toggle:wifi:") -> openSettingsPage(context, "wifi")
                payload.startsWith("toggle:bluetooth:") -> openSettingsPage(context, "bluetooth")
                payload.startsWith("toggle:volume:") -> applyMobileVolume(context, payload)
                payload.startsWith("toggle:brightness:") -> applyMobileBrightness(context, payload)
                else -> openSettingsPage(context, "")
            }
            onRefresh?.invoke()
            return true
        }
        if (r.category == "settings" || payload.startsWith("settings:")) {
            openSettingsPage(context, payload.removePrefix("settings:"))
            onRefresh?.invoke()
            return true
        }
        if (r.category == "config" || r.category == "setup" ||
            payload.startsWith("config:") || payload.startsWith("setup:")
        ) {
            // C++ edits/validates the mobile config; mirror any clipboard output.
            WilfredBridge.execute(index, "open")
            copyText(context, r.title)
            onRefresh?.invoke()
            return true
        }
        if (r.category == "plugins" || payload.startsWith("plugin_approve")) {
            val ok = WilfredBridge.execute(index, "open")
            Toast.makeText(
                context,
                if (ok) context.getString(R.string.action_done) else context.getString(R.string.action_failed),
                Toast.LENGTH_SHORT
            ).show()
            onRefresh?.invoke()
            return true
        }
        if (r.category == "layout" || payload.startsWith("tile:") ||
            payload.startsWith("layout_apply:")
        ) {
            Toast.makeText(context, context.getString(R.string.not_supported_desktop), Toast.LENGTH_SHORT).show()
            return true
        }
        return false
    }

    fun openSettingsPage(context: Context, page: String) {
        val action = when (page.lowercase()) {
            "wifi" -> Settings.ACTION_WIFI_SETTINGS
            "network" -> Settings.ACTION_WIRELESS_SETTINGS
            "bluetooth" -> Settings.ACTION_BLUETOOTH_SETTINGS
            "sound" -> Settings.ACTION_SOUND_SETTINGS
            "display" -> Settings.ACTION_DISPLAY_SETTINGS
            "battery", "power" -> Settings.ACTION_BATTERY_SAVER_SETTINGS
            "apps" -> Settings.ACTION_APPLICATION_SETTINGS
            "privacy" -> Settings.ACTION_PRIVACY_SETTINGS
            else -> Settings.ACTION_SETTINGS
        }
        try {
            context.startActivity(Intent(action).apply { addFlags(Intent.FLAG_ACTIVITY_NEW_TASK) })
        } catch (_: Exception) {
            try {
                context.startActivity(Intent(Settings.ACTION_SETTINGS).apply {
                    addFlags(Intent.FLAG_ACTIVITY_NEW_TASK)
                })
            } catch (_: Exception) { }
        }
    }

    private fun applyMobileVolume(context: Context, payload: String) {
        try {
            val am = context.getSystemService(Context.AUDIO_SERVICE) as AudioManager
            val op = payload.removePrefix("toggle:volume:")
            when {
                op.startsWith("set:") -> {
                    val level = op.removePrefix("set:").toIntOrNull() ?: return
                    val max = am.getStreamMaxVolume(AudioManager.STREAM_MUSIC)
                    am.setStreamVolume(AudioManager.STREAM_MUSIC, level * max / 100, 0)
                }
                op == "mute" -> am.adjustVolume(AudioManager.ADJUST_MUTE, 0)
                op == "unmute" -> am.adjustVolume(AudioManager.ADJUST_UNMUTE, 0)
                op == "mute_toggle" -> am.adjustVolume(AudioManager.ADJUST_TOGGLE_MUTE, 0)
                op == "up" -> am.adjustVolume(AudioManager.ADJUST_RAISE, AudioManager.FLAG_SHOW_UI)
                op == "down" -> am.adjustVolume(AudioManager.ADJUST_LOWER, AudioManager.FLAG_SHOW_UI)
            }
        } catch (_: Exception) { }
    }

    private fun applyMobileBrightness(context: Context, payload: String) {
        try {
            if (!Settings.System.canWrite(context)) {
                openSettingsPage(context, "display")
                return
            }
            val cur = Settings.System.getInt(
                context.contentResolver, Settings.System.SCREEN_BRIGHTNESS, 128
            )
            val op = payload.removePrefix("toggle:brightness:")
            val next = when {
                op.startsWith("set:") -> (op.removePrefix("set:").toIntOrNull() ?: return) * 255 / 100
                op == "up" -> cur + 255 / 10
                op == "down" -> cur - 255 / 10
                else -> return
            }.coerceIn(1, 255)
            Settings.System.putInt(context.contentResolver, Settings.System.SCREEN_BRIGHTNESS, next)
        } catch (_: Exception) { }
    }

    private fun copyableMini(r: SearchResult): Boolean {        // Timers/notes/todos/clips/process/media cards: primary tap copies the
        // display text; destructive ops live in the long-press menu.
        return r.category == "timer" || r.category == "stopwatch" || r.category == "note" ||
            r.category == "todo" || r.category == "clips" || r.category == "clipboard" ||
            r.category == "process" || r.category == "kill" || r.category == "media" ||
            r.category == "color" || r.kind == "uuid" || r.kind == "base64" ||
            r.kind == "sha256" || r.kind == "dev"
    }

    /** Long-press: show all C++-provided actions for this result. */
    fun showActionsDialog(context: Context, r: SearchResult, index: Int, onRefresh: (() -> Unit)? = null) {
        val items = WilfredBridge.actionsFor(index).ifEmpty { fallbackActions(r) }
        val labels = items.map { it.label }.toTypedArray()
        val dialog = AlertDialog.Builder(context)
            .setTitle(if (r.title.isNotBlank()) r.title else r.path)
            .setItems(labels) { _, which ->
                runAction(context, r, index, items[which].id, onRefresh)
            }
            .setNegativeButton(android.R.string.cancel, null)
            .create()
        // Services (floating popup) have no activity window token; show the
        // dialog as an overlay window instead.
        if (context !is android.app.Activity) {
            try {
                dialog.window?.setType(
                    if (android.os.Build.VERSION.SDK_INT >= 26)
                        android.view.WindowManager.LayoutParams.TYPE_APPLICATION_OVERLAY
                    else {
                        @Suppress("DEPRECATION")
                        android.view.WindowManager.LayoutParams.TYPE_PHONE
                    }
                )
            } catch (_: Exception) { }
        }
        try {
            dialog.show()
        } catch (_: Exception) {
            // Last resort on a Service context: run the primary action.
            openResult(context, r, "", index, onRefresh)
        }
    }

    private fun fallbackActions(r: SearchResult): List<ResultActionItem> {
        return listOf(ResultActionItem("open", "Open"), ResultActionItem("copy_text", "Copy"))
    }

    private fun runAction(context: Context, r: SearchResult, index: Int, actionId: String, onRefresh: (() -> Unit)?) {
        when {
            actionId == "copy_text" || actionId == "copy" || actionId == "paste" || actionId == "expand" ||
                actionId == "copy_path" || actionId == "copy_name" || actionId == "copy_file_uri" ||
                actionId == "copy_posix" || actionId == "copy_wsl" || actionId == "hash_file" -> {
                val ok = WilfredBridge.execute(index, actionId)
                // C++ updated its clipboard override; mirror to Android.
                val text = when (actionId) {
                    "copy_path" -> r.path.ifEmpty { r.payload }
                    "copy_name" -> r.title
                    "copy_file_uri" -> r.path.ifEmpty { r.payload }
                    else -> if (r.payload.isNotEmpty()) r.payload else r.title
                }
                if (ok || text.isNotEmpty()) copyText(context, text)
                onRefresh?.invoke()
            }
            actionId == "open" || actionId.startsWith("open_with:") -> {
                openResult(context, r, "", index, onRefresh)
            }
            actionId == "reveal" -> {
                // Android has no file-manager reveal; show parent folder.
                val parent = File(r.path).parent ?: r.path
                Toast.makeText(context, parent, Toast.LENGTH_LONG).show()
            }
            actionId == "timer_stop" || actionId.startsWith("note_delete:") ||
                actionId.startsWith("todo_done:") || actionId.startsWith("todo_undo:") ||
                actionId.startsWith("todo_delete:") || actionId == "clip_pin" ||
                actionId == "clip_unpin" || actionId == "clip_clear" ||
                actionId.startsWith("workflow:") -> {
                val ok = WilfredBridge.execute(index, actionId)
                Toast.makeText(
                    context,
                    if (ok) context.getString(R.string.action_done) else context.getString(R.string.action_failed),
                    Toast.LENGTH_SHORT
                ).show()
                onRefresh?.invoke()
            }
            actionId.startsWith("media:") || actionId == "kill_process" ||
                actionId.startsWith("window_") || actionId.startsWith("tile:") ||
                actionId.startsWith("layout_apply:") || actionId == "transcribe_run" ||
                actionId.startsWith("dictate_run") || actionId.startsWith("focus_window:") -> {
                Toast.makeText(context, context.getString(R.string.not_supported_desktop), Toast.LENGTH_SHORT).show()
            }
            else -> {
                val ok = WilfredBridge.execute(index, actionId)
                if (!ok) openResult(context, r, "", index, onRefresh)
                else onRefresh?.invoke()
            }
        }
    }

    /** F3-style peek: file/folder info + text head from C++ preview_json. */
    fun showPreviewDialog(context: Context, r: SearchResult) {
        val target = r.path.ifEmpty { r.payload }
        if (target.isBlank() || target.startsWith("package:") || target.startsWith("http")) {
            showMessageDialog(
                context,
                r.title,
                r.subtitle.ifBlank { r.payload }
            )
            return
        }
        val obj = WilfredBridge.preview(target) ?: JSONObject()
        val exists = obj.optBoolean("exists", false)
        val msg = if (!exists) {
            context.getString(R.string.preview_missing)
        } else {
            val isDir = obj.optBoolean("is_dir", false)
            val size = obj.optLong("size", 0)
            val preview = obj.optString("preview", "")
            val head = if (isDir) context.getString(R.string.preview_folder) + "\n" + preview
            else context.getString(R.string.preview_size, size) + "\n\n" + preview
            val sub = if (r.subtitle.isNotBlank()) r.subtitle + "\n\n" else ""
            sub + head
        }
        val dialog = AlertDialog.Builder(context)
            .setTitle(r.title.ifBlank { obj.optString("name", target) })
            .setMessage(msg.ifBlank { context.getString(R.string.preview_empty) })
            .setPositiveButton(android.R.string.ok, null)
            .setNeutralButton(R.string.copy_path) { _, _ -> copyText(context, target) }
            .create()
        if (context !is android.app.Activity) {
            try {
                dialog.window?.setType(
                    if (android.os.Build.VERSION.SDK_INT >= 26)
                        android.view.WindowManager.LayoutParams.TYPE_APPLICATION_OVERLAY
                    else {
                        @Suppress("DEPRECATION")
                        android.view.WindowManager.LayoutParams.TYPE_PHONE
                    }
                )
            } catch (_: Exception) { }
        }
        try {
            dialog.show()
        } catch (_: Exception) {
            Toast.makeText(context, msg, Toast.LENGTH_LONG).show()
        }
    }

    private fun showMessageDialog(context: Context, title: String, message: String) {
        val dialog = AlertDialog.Builder(context)
            .setTitle(title)
            .setMessage(message)
            .setPositiveButton(android.R.string.ok, null)
            .create()
        if (context !is android.app.Activity) {
            try {
                dialog.window?.setType(
                    if (android.os.Build.VERSION.SDK_INT >= 26)
                        android.view.WindowManager.LayoutParams.TYPE_APPLICATION_OVERLAY
                    else {
                        @Suppress("DEPRECATION")
                        android.view.WindowManager.LayoutParams.TYPE_PHONE
                    }
                )
            } catch (_: Exception) { }
        }
        try {
            dialog.show()
        } catch (_: Exception) {
            Toast.makeText(context, message, Toast.LENGTH_LONG).show()
        }
    }

    fun launchAppInfo(context: Context, r: SearchResult) {
        // App details for package: results.
        if (!r.path.startsWith("package:")) return
        val pkg = r.path.removePrefix("package:")
        try {
            val intent = if (android.os.Build.VERSION.SDK_INT >= 33) {
                Intent(android.provider.Settings.ACTION_APPLICATION_DETAILS_SETTINGS).apply {
                    data = Uri.parse("package:$pkg")
                    addFlags(Intent.FLAG_ACTIVITY_NEW_TASK)
                }
            } else {
                Intent(android.provider.Settings.ACTION_APPLICATION_DETAILS_SETTINGS).apply {
                    data = Uri.parse("package:$pkg")
                    addFlags(Intent.FLAG_ACTIVITY_NEW_TASK)
                }
            }
            context.startActivity(intent)
        } catch (_: Exception) {
            Toast.makeText(context, pkg, Toast.LENGTH_SHORT).show()
        }
    }

    @Suppress("DEPRECATION")
    fun getInstalledApps(pm: PackageManager): List<Triple<String, String, String>> {
        return try {
            val pkgs = if (android.os.Build.VERSION.SDK_INT >= 33) {
                pm.getInstalledPackages(PackageManager.PackageInfoFlags.of(0))
            } else {
                pm.getInstalledPackages(0)
            }
            pkgs.mapNotNull { pkg ->
                val label = pkg.applicationInfo?.loadLabel(pm)?.toString().orEmpty()
                val name = label.ifBlank { pkg.packageName }
                Triple(name, pkg.packageName, label)
            }
        } catch (_: Exception) { emptyList() }
    }
}
