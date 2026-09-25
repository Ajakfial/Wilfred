#include "wilfred/apps/discovery.hpp"

#include "wilfred/core/log.hpp"
#include "wilfred/core/paths.hpp"
#include "wilfred/fs/classify.hpp"
#include "wilfred/index/record.hpp"
#include "wilfred/platform/native.hpp"

namespace wilfred {

std::vector<AppInfo> discover_applications() {
  try {
    return native_discover_apps();
  } catch (...) {
    log_warn("apps", "application discovery threw");
    return {};
  }
}

void index_applications(IndexEngine& engine) {
  for (auto& app : discover_applications()) {
    IndexRecord rec;
    rec.kind = FileKind::Application;
    rec.flags = RecordFlags::Application | RecordFlags::Executable;
    if (app.system) rec.flags = rec.flags | RecordFlags::System;
    auto st = stat_path(app.path);
    rec.size = st.size;
    rec.mtime = st.mtime;
    rec.ctime = st.ctime;
    rec.atime = st.atime;
    engine.store().upsert(rec, app.path);
  }
  engine.bump_generation();
}

bool launch_path(const std::string& path) { return native_launch(path); }
bool reveal_path(const std::string& path) { return native_reveal(path); }
bool open_url(const std::string& url) { return native_open_url(url); }

}  // namespace wilfred
