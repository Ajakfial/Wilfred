// Wilfred Android JNI bridge: thin mutex-guarded wrapper around AndroidCore.
// All UI lives in Kotlin; all search/index logic lives in C++ (wilfred_core).
// Build with the Android NDK (see scripts/build-android.sh and
// android/app/build.gradle.kts).

#include <jni.h>

#include <mutex>
#include <string>

#include "wilfred/platform/android_bridge.hpp"

namespace {

wilfred::AndroidCore g_core;
std::mutex g_mu;

std::string jstring_to_utf8(JNIEnv* env, jstring s) {
  if (!s) return {};
  const char* chars = env->GetStringUTFChars(s, nullptr);
  if (!chars) return {};
  std::string out(chars);
  env->ReleaseStringUTFChars(s, chars);
  return out;
}

jstring utf8_to_jstring(JNIEnv* env, const std::string& s) { return env->NewStringUTF(s.c_str()); }

}  // namespace

extern "C" {

JNIEXPORT jboolean JNICALL Java_com_wilfred_launcher_WilfredBridge_nativeInit(JNIEnv* env,
                                                                              jobject /*thiz*/,
                                                                              jstring filesDir) {
  std::lock_guard<std::mutex> lock(g_mu);
  std::string dir = jstring_to_utf8(env, filesDir);
  std::string error;
  return g_core.boot(dir, error) ? JNI_TRUE : JNI_FALSE;
}

JNIEXPORT jstring JNICALL Java_com_wilfred_launcher_WilfredBridge_nativeSearch(JNIEnv* env,
                                                                                jobject /*thiz*/,
                                                                                jstring query,
                                                                                jint limit) {
  std::lock_guard<std::mutex> lock(g_mu);
  std::string q = jstring_to_utf8(env, query);
  return utf8_to_jstring(env, g_core.search_json(q, static_cast<int>(limit)));
}

JNIEXPORT jstring JNICALL Java_com_wilfred_launcher_WilfredBridge_nativeStatus(JNIEnv* env,
                                                                               jobject /*thiz*/) {
  std::lock_guard<std::mutex> lock(g_mu);
  return utf8_to_jstring(env, g_core.status_json());
}

JNIEXPORT jboolean JNICALL Java_com_wilfred_launcher_WilfredBridge_nativeIndexNow(JNIEnv*,
                                                                                   jobject /*thiz*/) {
  std::lock_guard<std::mutex> lock(g_mu);
  std::string error;
  return g_core.index_now(error) ? JNI_TRUE : JNI_FALSE;
}

JNIEXPORT jboolean JNICALL Java_com_wilfred_launcher_WilfredBridge_nativeRegisterApp(
    JNIEnv* env, jobject /*thiz*/, jstring name, jstring packageId, jstring label) {
  std::lock_guard<std::mutex> lock(g_mu);
  return g_core.register_app(jstring_to_utf8(env, name), jstring_to_utf8(env, packageId),
                             jstring_to_utf8(env, label))
             ? JNI_TRUE
             : JNI_FALSE;
}

JNIEXPORT jboolean JNICALL Java_com_wilfred_launcher_WilfredBridge_nativeRecordChoice(
    JNIEnv* env, jobject /*thiz*/, jstring query, jstring key) {
  std::lock_guard<std::mutex> lock(g_mu);
  return g_core.record_choice(jstring_to_utf8(env, query), jstring_to_utf8(env, key)) ? JNI_TRUE
                                                                                      : JNI_FALSE;
}

JNIEXPORT jstring JNICALL Java_com_wilfred_launcher_WilfredBridge_nativeAssist(JNIEnv* env,
                                                                                jobject /*thiz*/,
                                                                                jstring query) {
  std::lock_guard<std::mutex> lock(g_mu);
  return utf8_to_jstring(env, g_core.assist_json(jstring_to_utf8(env, query)));
}

JNIEXPORT jstring JNICALL Java_com_wilfred_launcher_WilfredBridge_nativeActions(JNIEnv* env,
                                                                                jobject /*thiz*/,
                                                                                jint index) {
  std::lock_guard<std::mutex> lock(g_mu);
  return utf8_to_jstring(env, g_core.actions_json(static_cast<std::size_t>(index)));
}

JNIEXPORT jboolean JNICALL Java_com_wilfred_launcher_WilfredBridge_nativeExecute(
    JNIEnv* env, jobject /*thiz*/, jint index, jstring actionId) {
  std::lock_guard<std::mutex> lock(g_mu);
  std::string error;
  return g_core.execute_action(static_cast<std::size_t>(index), jstring_to_utf8(env, actionId),
                               error)
             ? JNI_TRUE
             : JNI_FALSE;
}

JNIEXPORT void JNICALL Java_com_wilfred_launcher_WilfredBridge_nativeSetClipboard(
    JNIEnv* env, jobject /*thiz*/, jstring text) {
  std::lock_guard<std::mutex> lock(g_mu);
  g_core.set_clipboard(jstring_to_utf8(env, text));
}

JNIEXPORT jstring JNICALL Java_com_wilfred_launcher_WilfredBridge_nativePreview(
    JNIEnv* env, jobject /*thiz*/, jstring path) {
  std::lock_guard<std::mutex> lock(g_mu);
  return utf8_to_jstring(env, g_core.preview_json(jstring_to_utf8(env, path)));
}

}  // extern "C"
