#include "test.hpp"
#include "wilfred/ipc/protocol.hpp"

void test_protocol() {
  using namespace wilfred;
  IpcRequest req;
  CHECK(decode_request("SEARCH\tfirefox", req));
  CHECK_EQ(req.cmd, "search");
  CHECK_EQ(req.query, "firefox");

  CHECK(decode_request("LAUNCH\t/usr/bin/firefox", req));
  CHECK_EQ(req.cmd, "launch");
  CHECK_EQ(req.path, "/usr/bin/firefox");

  CHECK(decode_request("STATUS", req));
  CHECK_EQ(req.cmd, "status");

  CHECK(decode_request("BACKUP\tC:/tmp/w.bk", req));
  CHECK_EQ(req.cmd, "backup");
  CHECK_EQ(req.path, "C:/tmp/w.bk");

  CHECK(decode_request("{\"cmd\":\"search\",\"query\":\"code\",\"limit\":10}", req));
  CHECK_EQ(req.cmd, "search");
  CHECK_EQ(req.query, "code");
  CHECK_EQ(req.limit, 10);

  IpcResponse resp;
  resp.ok = true;
  SearchResult r;
  r.id = 1;
  r.score = 99;
  r.title = "Firefox";
  r.path = "/apps/firefox";
  r.kind = FileKind::Application;
  r.actions.push_back({"reveal", "Show in folder"});
  resp.results.push_back(r);
  auto json = encode_response(resp);
  CHECK(json.find("\"ok\":true") != std::string::npos);
  CHECK(json.find("Firefox") != std::string::npos);
  CHECK(json.find("application") != std::string::npos);
  CHECK(json.find("\"actions\"") != std::string::npos);
  CHECK(json.find("reveal") != std::string::npos);

  IpcRequest bad;
  CHECK(!decode_request("{}", bad));
}
