// Wilfred iOS core bridge (pure ObjC header; implementation is ObjC++).
// All UI lives in Swift; all search/index logic lives in C++
// (wilfred_core, MobileCore). Mirrors the Android JNI layer
// (android/app/src/main/cpp/wilfred_jni.cpp) method for method.
#import <Foundation/Foundation.h>

NS_ASSUME_NONNULL_BEGIN

@interface WilfredCoreBridge : NSObject

// Boots config/index/history/snippets/plugins/providers from the app
// sandbox documents dir. Returns YES on success; error holds the reason.
- (BOOL)bootWithFilesDir:(NSString*)dir error:(NSString* _Nullable* _Nullable)error;

// JSON array of {title,subtitle,path,payload,score,action,category,kind,
// actions:[{id,label}]}. Never nil; @"[]" on any failure.
- (NSString*)searchJSON:(NSString*)query limit:(NSInteger)limit;

// Assist payload {"correction":"","ghost":"","candidates":[]}.
- (NSString*)assistJSON:(NSString*)query;

// Actions [{"id":"","label":""}] for a result of the last searchJSON call.
- (NSString*)actionsJSON:(NSInteger)index;

// Runs a C++ side-effect action (timer_stop, note_delete:*, todo_done:*,
// clip_pin, transcribe_run, copy_*, ...) on a stored last-search result.
- (BOOL)executeAction:(NSInteger)index
             actionId:(NSString*)actionId
                error:(NSString* _Nullable* _Nullable)error;

- (NSString*)statusJSON;
- (BOOL)indexNow:(NSString* _Nullable* _Nullable)error;
- (BOOL)recordChoice:(NSString*)query key:(NSString*)key;

// Pushes the current UIPasteboard text into the C++ core (clipboard minis,
// snippet `{clipboard}` expansion, path hints).
- (void)setClipboard:(NSString*)text;

// File preview {"exists":true,"is_dir":false,"size":0,"name":"","preview":""}.
- (NSString*)previewJSON:(NSString*)path;

@end

NS_ASSUME_NONNULL_END
