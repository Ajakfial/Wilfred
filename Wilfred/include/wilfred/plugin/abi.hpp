#pragma once

// Native plugin ABI (C). Loadable as .dll / .so / .dylib.
// Query JSON: {"op":"query","q":"...","limit":40}
// Result JSON: {"results":[{"title":"...","subtitle":"...","path":"...","score":100,"action":"open"}]}

#ifdef __cplusplus
extern "C" {
#endif

#define WILFRED_PLUGIN_ABI 1

typedef int (*wilfred_plugin_abi_fn)(void);
typedef const char* (*wilfred_plugin_id_fn)(void);
typedef const char* (*wilfred_plugin_query_fn)(const char* json_request);
typedef const char* (*wilfred_plugin_exec_fn)(const char* json_request);

#ifdef __cplusplus
}
#endif
