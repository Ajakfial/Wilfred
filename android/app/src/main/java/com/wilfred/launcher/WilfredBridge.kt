package com.wilfred.launcher

import org.json.JSONArray
import org.json.JSONObject

data class ResultActionItem(
    val id: String,
    val label: String
)

data class SearchResult(
    val title: String,
    val subtitle: String,
    val path: String,
    val payload: String,
    val score: Int,
    val action: String,
    val category: String,
    val kind: String,
    val actions: List<ResultActionItem> = emptyList()
)

data class Assist(
    val correction: String = "",
    val ghost: String = "",
    val candidates: List<String> = emptyList()
)

object WilfredBridge {
    init {
        System.loadLibrary("wilfred_jni")
    }

    @Volatile var ready: Boolean = false
        private set

    fun init(filesDir: String): Boolean {
        if (ready) return true
        ready = nativeInit(filesDir)
        return ready
    }

    fun search(query: String, limit: Int = 40): List<SearchResult> {
        if (!ready) return emptyList()
        return parseResults(nativeSearch(query, limit))
    }

    fun assist(query: String): Assist {
        if (!ready) return Assist()
        return parseAssist(nativeAssist(query))
    }

    fun actionsFor(index: Int): List<ResultActionItem> {
        if (!ready) return emptyList()
        return parseActions(nativeActions(index))
    }

    /** Run a C++ side-effect action (timer_stop, note_delete:*, todo_done:*, clip_*, ...). */
    fun execute(index: Int, actionId: String): Boolean {
        if (!ready) return false
        return nativeExecute(index, actionId)
    }

    fun status(): String {
        if (!ready) return "{\"ok\":false}"
        return nativeStatus()
    }

    fun indexNow(): Boolean = ready && nativeIndexNow()

    fun registerApp(name: String, packageId: String, label: String = ""): Boolean {
        if (!ready) return false
        return nativeRegisterApp(name, packageId, label)
    }

    fun recordChoice(query: String, key: String): Boolean {
        if (!ready) return false
        return nativeRecordChoice(query, key)
    }

    fun pushClipboard(text: String) {
        if (!ready) return
        try { nativeSetClipboard(text) } catch (_: Exception) { }
    }

    fun preview(path: String): JSONObject? {
        if (!ready) return null
        return try { JSONObject(nativePreview(path)) } catch (_: Exception) { null }
    }

    internal fun parseResults(json: String): List<SearchResult> {
        if (json.isBlank()) return emptyList()
        val arr = try { JSONArray(json) } catch (_: Exception) { return emptyList() }
        val out = ArrayList<SearchResult>(arr.length())
        for (i in 0 until arr.length()) {
            val o = arr.optJSONObject(i) ?: continue
            val actions = ArrayList<ResultActionItem>()
            val ja = o.optJSONArray("actions")
            if (ja != null) {
                for (j in 0 until ja.length()) {
                    val a = ja.optJSONObject(j) ?: continue
                    val id = a.optString("id")
                    if (id.isEmpty()) continue
                    actions.add(ResultActionItem(id, a.optString("label", id)))
                }
            }
            out.add(
                SearchResult(
                    title = o.optString("title"),
                    subtitle = o.optString("subtitle"),
                    path = o.optString("path"),
                    payload = o.optString("payload"),
                    score = o.optInt("score"),
                    action = o.optString("action"),
                    category = o.optString("category"),
                    kind = o.optString("kind"),
                    actions = actions
                )
            )
        }
        return out
    }

    internal fun parseAssist(json: String): Assist {
        if (json.isBlank()) return Assist()
        return try {
            val o = JSONObject(json)
            val cands = ArrayList<String>()
            val ja = o.optJSONArray("candidates")
            if (ja != null) for (i in 0 until ja.length()) {
                val s = ja.optString(i)
                if (s.isNotEmpty()) cands.add(s)
            }
            Assist(
                correction = o.optString("correction", ""),
                ghost = o.optString("ghost", ""),
                candidates = cands
            )
        } catch (_: Exception) { Assist() }
    }

    internal fun parseActions(json: String): List<ResultActionItem> {
        if (json.isBlank()) return emptyList()
        return try {
            val arr = JSONArray(json)
            val out = ArrayList<ResultActionItem>(arr.length())
            for (i in 0 until arr.length()) {
                val o = arr.optJSONObject(i) ?: continue
                val id = o.optString("id")
                if (id.isEmpty()) continue
                out.add(ResultActionItem(id, o.optString("label", id)))
            }
            out
        } catch (_: Exception) { emptyList() }
    }

    private external fun nativeInit(filesDir: String): Boolean
    private external fun nativeSearch(query: String, limit: Int): String
    private external fun nativeStatus(): String
    private external fun nativeIndexNow(): Boolean
    private external fun nativeRegisterApp(name: String, packageId: String, label: String): Boolean
    private external fun nativeRecordChoice(query: String, key: String): Boolean
    private external fun nativeAssist(query: String): String
    private external fun nativeActions(index: Int): String
    private external fun nativeExecute(index: Int, actionId: String): Boolean
    private external fun nativeSetClipboard(text: String)
    private external fun nativePreview(path: String): String
}
