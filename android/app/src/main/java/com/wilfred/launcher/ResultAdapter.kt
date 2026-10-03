package com.wilfred.launcher

import android.view.LayoutInflater
import android.view.View
import android.view.ViewGroup
import android.widget.TextView
import androidx.recyclerview.widget.RecyclerView

class ResultAdapter(
    private val onClick: (SearchResult, Int) -> Unit,
    private val onLongClick: (SearchResult, Int) -> Unit = { _, _ -> }
) : RecyclerView.Adapter<ResultAdapter.Holder>() {

    private var items: List<SearchResult> = emptyList()

    class Holder(v: View) : RecyclerView.ViewHolder(v) {
        val title: TextView = v.findViewById(R.id.row_title)
        val subtitle: TextView = v.findViewById(R.id.row_subtitle)
        val badge: TextView = v.findViewById(R.id.row_badge)
    }

    fun submit(next: List<SearchResult>) {
        items = next
        notifyDataSetChanged()
    }

    fun itemAt(position: Int): SearchResult? = items.getOrNull(position)

    override fun onCreateViewHolder(parent: ViewGroup, viewType: Int): Holder {
        val v = LayoutInflater.from(parent.context).inflate(R.layout.row_result, parent, false)
        return Holder(v)
    }

    override fun getItemCount(): Int = items.size

    override fun onBindViewHolder(holder: Holder, position: Int) {
        val r = items[position]
        holder.title.text = r.title.ifBlank { r.path.ifBlank { r.payload } }
        holder.subtitle.text = r.subtitle.ifBlank { r.path.ifBlank { r.payload } }
        holder.subtitle.visibility = if (holder.subtitle.text.isBlank()) View.GONE else View.VISIBLE
        val badge = when {
            r.category.isNotEmpty() -> r.category
            r.action.isNotEmpty() -> r.action
            r.kind.isNotEmpty() -> r.kind
            else -> ""
        }
        holder.badge.text = badge
        holder.badge.visibility = if (badge.isEmpty()) View.GONE else View.VISIBLE
        holder.itemView.setOnClickListener { onClick(r, holder.bindingAdapterPosition) }
        holder.itemView.setOnLongClickListener {
            val pos = holder.bindingAdapterPosition
            if (pos != RecyclerView.NO_POSITION) onLongClick(items[pos], pos)
            true
        }
    }
}
