package com.wilfred.launcher

import android.content.Intent
import android.net.Uri
import android.os.Bundle
import android.os.Handler
import android.os.Looper
import android.provider.Settings
import android.text.Editable
import android.text.TextWatcher
import android.view.Menu
import android.view.MenuItem
import android.view.View
import android.widget.EditText
import android.widget.LinearLayout
import android.widget.ProgressBar
import android.widget.TextView
import android.widget.Toast
import androidx.appcompat.app.AppCompatActivity
import androidx.recyclerview.widget.LinearLayoutManager
import androidx.recyclerview.widget.RecyclerView
import java.util.concurrent.Executors

/**
 * Wilfred search UI (Kotlin). All search/rank/calc/snippets/notes/timers
 * logic runs in C++ (wilfred_core via [WilfredBridge]); this activity only
 * owns the search bar, assist strip, results list and Android Intents.
 */
class MainActivity : AppCompatActivity() {
    private val io = Executors.newSingleThreadExecutor()
    private val main = Handler(Looper.getMainLooper())
    private lateinit var adapter: ResultAdapter
    private var lastQuery: String = ""
    private var lastResults: List<SearchResult> = emptyList()
    private var bootFailed: Boolean = false
    private var searchSeq: Int = 0

    private lateinit var input: EditText
    private lateinit var correctionBar: TextView
    private lateinit var ghostLine: TextView
    private lateinit var candidatesRow: LinearLayout
    private lateinit var statusLine: TextView

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        setContentView(R.layout.activity_main)

        input = findViewById(R.id.search_input)
        val list = findViewById<RecyclerView>(R.id.result_list)
        statusLine = findViewById(R.id.status_line)
        val progress = findViewById<ProgressBar>(R.id.boot_progress)
        correctionBar = findViewById(R.id.correction_bar)
        ghostLine = findViewById(R.id.ghost_line)
        candidatesRow = findViewById(R.id.candidates_row)

        adapter = ResultAdapter(
            onClick = { r, idx -> WilfredActions.openResult(this, r, lastQuery, idx) { refresh() } },
            onLongClick = { r, idx ->
                WilfredActions.showActionsDialog(this, r, idx) { refresh() }
            }
        )
        list.layoutManager = LinearLayoutManager(this)
        list.adapter = adapter

        progress.visibility = View.VISIBLE
        statusLine.text = getString(R.string.status_booting)
        io.execute {
            val ok = WilfredBridge.init(filesDir.absolutePath)
            if (ok) {
                WilfredActions.androidClipboardText(this@MainActivity).let {
                    if (it.isNotEmpty()) WilfredBridge.pushClipboard(it)
                }
                indexInstalledApps()
            }
            main.post {
                progress.visibility = View.GONE
                if (!ok) {
                    bootFailed = true
                    statusLine.text = getString(R.string.status_boot_failed)
                } else {
                    statusLine.text = prettyStatus(WilfredBridge.status())
                    runSearch("", input)
                    ensureFloatingButton()
                }
            }
        }

        input.addTextChangedListener(object : TextWatcher {
            override fun beforeTextChanged(s: CharSequence?, a: Int, b: Int, c: Int) = Unit
            override fun onTextChanged(s: CharSequence?, a: Int, b: Int, c: Int) = Unit
            override fun afterTextChanged(s: Editable?) {
                runSearch(s?.toString().orEmpty(), input)
            }
        })
        input.setOnEditorActionListener { _, _, _ ->
            // Enter opens the top result, like the desktop overlay.
            val top = lastResults.firstOrNull()
            if (top != null) WilfredActions.openResult(this, top, lastQuery, 0) { refresh() }
            true
        }
        correctionBar.setOnClickListener {
            val fix = correctionBar.tag as? String
            if (!fix.isNullOrEmpty()) input.setText(fix).also { input.setSelection(fix.length) }
        }
    }

    override fun onResume() {
        super.onResume()
        // Sync the Android clipboard into C++ before each visible search so
        // clipboard minis and `{clipboard}` snippet placeholders stay fresh.
        io.execute {
            val clip = WilfredActions.androidClipboardText(this)
            if (clip.isNotEmpty()) WilfredBridge.pushClipboard(clip)
        }
        findViewById<EditText>(R.id.search_input)?.requestFocus()
    }

    override fun onNewIntent(intent: Intent) {
        super.onNewIntent(intent)
        setIntent(intent)
        findViewById<EditText>(R.id.search_input)?.requestFocus()
    }

    override fun onCreateOptionsMenu(menu: Menu): Boolean {
        menuInflater.inflate(R.menu.main_menu, menu)
        return true
    }

    override fun onOptionsItemSelected(item: MenuItem): Boolean {
        return when (item.itemId) {
            R.id.menu_index -> {
                Toast.makeText(this, getString(R.string.indexing), Toast.LENGTH_SHORT).show()
                io.execute {
                    val ok = WilfredBridge.indexNow()
                    main.post {
                        Toast.makeText(
                            this,
                            if (ok) getString(R.string.index_done) else getString(R.string.index_failed),
                            Toast.LENGTH_SHORT
                        ).show()
                        refresh()
                    }
                }
                true
            }
            R.id.menu_floating -> {
                ensureFloatingButton(requestPermission = true)
                true
            }
            R.id.menu_status -> {
                statusLine.text = prettyStatus(WilfredBridge.status())
                true
            }
            else -> super.onOptionsItemSelected(item)
        }
    }

    private fun runSearch(query: String, input: EditText) {
        lastQuery = query
        if (bootFailed) return
        val seq = ++searchSeq
        io.execute {
            val results = WilfredBridge.search(query, 40)
            val assist = WilfredBridge.assist(query)
            main.post {
                if (seq != searchSeq) return@post
                if (query != input.text.toString()) return@post
                lastResults = results
                adapter.submit(results)
                renderAssist(query, assist)
                findViewById<TextView>(R.id.status_line)?.text =
                    if (query.isBlank()) prettyStatus(WilfredBridge.status())
                    else if (results.isEmpty()) getString(R.string.no_results)
                    else resources.getQuantityString(R.plurals.results_n, results.size, results.size)
            }
        }
    }

    private fun renderAssist(query: String, assist: Assist) {
        if (query.isBlank()) {
            correctionBar.visibility = View.GONE
            ghostLine.visibility = View.GONE
            candidatesRow.visibility = View.GONE
            return
        }
        if (assist.correction.isNotEmpty()) {
            correctionBar.visibility = View.VISIBLE
            correctionBar.tag = assist.correction
            correctionBar.text = getString(R.string.did_you_mean, assist.correction)
        } else {
            correctionBar.visibility = View.GONE
        }
        if (assist.ghost.isNotEmpty() && !assist.ghost.equals(query, ignoreCase = true)) {
            ghostLine.visibility = View.VISIBLE
            ghostLine.text = assist.ghost
            ghostLine.setOnClickListener {
                input.setText(assist.ghost)
                input.setSelection(assist.ghost.length)
            }
        } else {
            ghostLine.visibility = View.GONE
        }
        candidatesRow.removeAllViews()
        if (assist.candidates.isNotEmpty()) {
            candidatesRow.visibility = View.VISIBLE
            for (c in assist.candidates.take(6)) {
                val tv = TextView(this).apply {
                    text = c
                    setPadding(24, 12, 24, 12)
                    setOnClickListener {
                        input.setText(c)
                        input.setSelection(c.length)
                    }
                }
                candidatesRow.addView(tv)
            }
        } else {
            candidatesRow.visibility = View.GONE
        }
    }

    private fun refresh() = runSearch(input.text.toString(), input)

    private fun prettyStatus(json: String): String {
        return try {
            val o = org.json.JSONObject(json)
            if (!o.optBoolean("ok", false)) return getString(R.string.status_boot_failed)
            val r = o.optInt("records", 0)
            val a = o.optInt("apps", 0)
            getString(R.string.status_ready, r, a)
        } catch (_: Exception) { json }
    }

    private fun indexInstalledApps() {
        try {
            for ((name, pkg, label) in WilfredActions.getInstalledApps(packageManager)) {
                WilfredBridge.registerApp(name, pkg, label)
            }
            WilfredBridge.indexNow()
        } catch (_: Exception) { }
    }

    private fun ensureFloatingButton(requestPermission: Boolean = false) {
        if (Settings.canDrawOverlays(this)) {
            FloatingWService.startIfPermitted(this)
            return
        }
        if (!requestPermission) return
        try {
            val intent = Intent(Settings.ACTION_MANAGE_OVERLAY_PERMISSION, Uri.parse("package:$packageName"))
            startActivity(intent)
            Toast.makeText(this, getString(R.string.overlay_grant), Toast.LENGTH_LONG).show()
        } catch (_: Exception) { }
    }
}
