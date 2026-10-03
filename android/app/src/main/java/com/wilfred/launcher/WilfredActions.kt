package com.wilfred.launcher

import android.app.AlertDialog
import android.content.ClipData
import android.content.ClipboardManager
import android.content.Context
import android.content.Intent
import android.content.pm.PackageManager
import android.net.Uri
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
                    }
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

    private fun copyableMini(r: SearchResult): Boolean {
        // Timers/notes/todos/clips/process/media cards: primary tap copies the
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
                actionId.startsWith("layout_apply:") || actionId.startsWith("workflow:") -> {
                val ok = WilfredBridge.execute(index, actionId)
                Toast.makeText(
                    context,
                    if (ok) context.getString(R.string.action_done) else context.getString(R.string.action_failed),
                    Toast.LENGTH_SHORT
                ).show()
                onRefresh?.invoke()
            }
            actionId.startsWith("media:") || actionId == "kill_process" ||
                actionId.startsWith("window_") || actionId == "transcribe_run" ||
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
