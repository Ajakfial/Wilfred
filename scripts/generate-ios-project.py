#!/usr/bin/env python3
"""Generate ios/Wilfred.xcodeproj/project.pbxproj deterministically.

The C++ file list is parsed from the repo-root CMakeLists.txt
(WILFRED_CORE_SOURCES plus the elseif(IOS) backend block), so the Xcode
project can never drift from the CMake build. Swift/ObjC++ sources are
scanned from ios/Wilfred. UUIDs derive from file paths (stable across
regenerations). Run:

    python3 scripts/generate-ios-project.py [--check]

--check only validates (referenced files exist, UUIDs unique) without
writing. scripts/build-ios.sh regenerates before every CI build.
"""

import hashlib
import os
import re
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
CMAKE = os.path.join(ROOT, "CMakeLists.txt")
IOS_DIR = os.path.join(ROOT, "ios", "Wilfred")
PROJ_DIR = os.path.join(ROOT, "ios", "Wilfred.xcodeproj")
PBXPROJ = os.path.join(PROJ_DIR, "project.pbxproj")


def uuid_for(key: str) -> str:
    return hashlib.sha1(("wilfred-ios:" + key).encode()).hexdigest()[:24].upper()


def parse_cmake_sources() -> "list[str]":
    """Base wilfred_core .cpp list from set(WILFRED_CORE_SOURCES ...)."""
    with open(CMAKE, encoding="utf-8") as f:
        text = f.read()
    m = re.search(r"set\(WILFRED_CORE_SOURCES\s*(.*?)\)", text, re.S)
    assert m, "WILFRED_CORE_SOURCES not found"
    files = re.findall(r"[A-Za-z0-9_./+-]+\.(?:cpp|mm|c)", m.group(1))
    assert files, "no sources parsed"
    return files


def parse_ios_backend() -> "list[str]":
    """src/platform/ios/* list from the elseif(IOS) block."""
    with open(CMAKE, encoding="utf-8") as f:
        text = f.read()
    m = re.search(r"^elseif\(IOS\)(.*?)^elseif\(APPLE\)", text, re.S | re.M)
    assert m, "elseif(IOS) block not found"
    files = re.findall(r"src/platform/ios/[A-Za-z0-9_./+-]+\.(?:cpp|mm|c)", m.group(1))
    assert files, "no ios backend files parsed"
    return files


def scan_ios_app() -> "list[str]":
    """Swift/ObjC++/ObjC app sources, repo-root-relative, sorted."""
    out = []
    for name in sorted(os.listdir(IOS_DIR)):
        if name.endswith((".swift", ".mm", ".m")):
            out.append("ios/Wilfred/" + name)
    assert out, "no app sources found"
    return out


def file_type(path: str) -> str:
    if path.endswith(".swift"):
        return "sourcecode.swift"
    if path.endswith(".mm"):
        return "sourcecode.cpp.objcpp"
    if path.endswith(".m"):
        return "sourcecode.c.objc"
    if path.endswith(".h"):
        return "sourcecode.c.h"
    return "sourcecode.cpp.cpp"


def main() -> int:
    check_only = "--check" in sys.argv
    core = parse_cmake_sources()
    backend = parse_ios_backend()
    app = scan_ios_app()

    # (repo-relative path, group path shown in Xcode, build phase or None)
    entries: "list[tuple[str, str, str | None]]" = []
    for p in core:
        entries.append((p, p, "sources"))
    for p in backend:
        entries.append((p, p, "sources"))
    for p in app:
        entries.append((p, "Wilfred/" + os.path.basename(p), "sources"))
    entries.append(("ios/Wilfred/WilfredCoreBridge.h", "Wilfred/WilfredCoreBridge.h", None))
    entries.append(
        ("ios/Wilfred/Wilfred-Bridging-Header.h", "Wilfred/Wilfred-Bridging-Header.h", None)
    )
    entries.append(("ios/Wilfred/Info.plist", "Wilfred/Info.plist", None))
    entries.append(("ios/Wilfred/Assets.xcassets", "Wilfred/Assets.xcassets", "resources"))

    # Validate every referenced file exists on disk.
    missing = [p for p, _, _ in entries if not os.path.exists(os.path.join(ROOT, *p.split("/")))]
    if missing:
        print("missing files:", *missing, sep="\n  ")
        return 1

    uuids = [uuid_for(f"buildfile:{p}") for p, _, _ in entries]
    uuids += [uuid_for(f"fileref:{p}") for p, _, _ in entries]
    assert len(set(uuids)) == len(uuids), "UUID collision"

    # Group tree mirroring directories. Group "src" points at ../../src
    # (project lives in ios/); "Wilfred" is relative to the project dir;
    # nested groups resolve against their parent. File refs use basenames.
    ancestors: "set[str]" = set()
    for _, shown, _ in entries:
        d = os.path.dirname(shown)
        while d:
            ancestors.add(d)
            d = os.path.dirname(d)
    group_uuid = {d: uuid_for("group:" + d) for d in ancestors}

    def direct_files(d: str) -> "list[str]":
        return [p for p, shown, _ in entries if os.path.dirname(shown) == d]

    def direct_subs(d: str) -> "list[str]":
        out = []
        for k in ancestors:
            if k != d and os.path.dirname(k) == d:
                out.append(k)
        return sorted(out)

    def group_path_block(d: str) -> str:
        if d == "src":
            # SOURCE_ROOT is the .xcodeproj dir (ios/), so ../src is the
            # repo-root src/.
            return 'path = ../src; sourceTree = SOURCE_ROOT;'
        if "/" not in d:
            return f'path = {d}; sourceTree = "<group>";'
        return f'name = {os.path.basename(d)}; path = {os.path.basename(d)}; ' \
            f'sourceTree = "<group>";'

    top_groups = sorted({d for d in ancestors if "/" not in d})
    lines: "list[str]" = []
    add = lines.append
    add("// !$*UTF8*$!")
    add("{")
    add("\tarchiveVersion = 1;")
    add("\tclasses = {")
    add("\t};")
    add("\tobjectVersion = 56;")
    add("\tobjects = {")

    def begin(section: str) -> None:
        add("")
        add(f"/* Begin {section} section */")

    def end(section: str) -> None:
        add(f"/* End {section} section */")

    proj = uuid_for("project")
    main_group = uuid_for("maingroup")
    products_group = uuid_for("products")
    target = uuid_for("target:Wilfred")
    app_ref = uuid_for("fileref:product:Wilfred.app")
    cfg_list_proj = uuid_for("cfglist:project")
    cfg_list_target = uuid_for("cfglist:target")
    cfg_proj_dbg = uuid_for("cfg:project:debug")
    cfg_proj_rel = uuid_for("cfg:project:release")
    cfg_tgt_dbg = uuid_for("cfg:target:debug")
    cfg_tgt_rel = uuid_for("cfg:target:release")
    phase_sources = uuid_for("phase:sources")
    phase_frameworks = uuid_for("phase:frameworks")
    phase_resources = uuid_for("phase:resources")

    # PBXBuildFile
    begin("PBXBuildFile")
    for p, shown, phase in entries:
        if phase is None:
            continue
        add(f"\t\t{uuid_for('buildfile:' + p)} /* {os.path.basename(p)} in "
            f"{'Sources' if phase == 'sources' else 'Resources'} */ = {{isa = PBXBuildFile; "
            f"fileRef = {uuid_for('fileref:' + p)} /* {os.path.basename(p)} */; }};")
    end("PBXBuildFile")

    # PBXFileReference
    begin("PBXFileReference")
    for p, shown, _ in entries:
        base = os.path.basename(p)
        if p.endswith(".xcassets"):
            add(f"\t\t{uuid_for('fileref:' + p)} /* Assets.xcassets */ = {{isa = PBXFileReference; "
                f"lastKnownFileType = folder.assetcatalog; path = Assets.xcassets; "
                f"sourceTree = \"<group>\"; }};")
        elif base == "Info.plist":
            add(f"\t\t{uuid_for('fileref:' + p)} /* Info.plist */ = {{isa = PBXFileReference; "
                f"lastKnownFileType = text.plist.xml; path = Info.plist; "
                f"sourceTree = \"<group>\"; }};")
        else:
            add(f"\t\t{uuid_for('fileref:' + p)} /* {shown} */ = {{isa = PBXFileReference; "
                f"lastKnownFileType = {file_type(p)}; path = {base}; "
                f"sourceTree = \"<group>\"; }};")
    add(f"\t\t{app_ref} /* Wilfred.app */ = {{isa = PBXFileReference; "
        f"explicitFileType = wrapper.application; path = Wilfred.app; "
        f"sourceTree = BUILT_PRODUCTS_DIR; }};")
    end("PBXFileReference")

    # PBXFrameworksBuildPhase / Resources / Sources
    begin("PBXFrameworksBuildPhase")
    add(f"\t\t{phase_frameworks} /* Frameworks */ = {{isa = PBXFrameworksBuildPhase; "
        f"buildActionMask = 2147483647; files = (); runOnlyForDeploymentPostprocessing = 0; }};")
    end("PBXFrameworksBuildPhase")

    begin("PBXGroup")
    for d in sorted(ancestors):
        kids = [uuid_for(f"fileref:{p}") for p in direct_files(d)]
        kids += [group_uuid[s] for s in direct_subs(d)]
        add(f"\t\t{group_uuid[d]} /* {d} */ = {{isa = PBXGroup; children = ("
            + ", ".join(kids) + "); " + group_path_block(d) + " };")
    add(f"\t\t{main_group} = {{isa = PBXGroup; children = ("
        + ", ".join([group_uuid[g] for g in top_groups] + [products_group])
        + "); sourceTree = \"<group>\"; };")
    add(f"\t\t{products_group} /* Products */ = {{isa = PBXGroup; children = ({app_ref},); "
        f"name = Products; sourceTree = \"<group>\"; }};")
    end("PBXGroup")

    begin("PBXNativeTarget")
    add(f"\t\t{target} /* Wilfred */ = {{isa = PBXNativeTarget; buildConfigurationList = "
        f"{cfg_list_target} /* Build configuration list for PBXNativeTarget \"Wilfred\" */; "
        f"buildPhases = ({phase_sources} /* Sources */, {phase_frameworks} /* Frameworks */, "
        f"{phase_resources} /* Resources */); buildRules = (); dependencies = (); "
        f"name = Wilfred; productName = Wilfred; productReference = {app_ref} /* Wilfred.app */; "
        f"productType = \"com.apple.product-type.application\"; }};")
    end("PBXNativeTarget")

    begin("PBXProject")
    add(f"\t\t{proj} /* Project object */ = {{isa = PBXProject; attributes = "
        f"{{LastUpgradeCheck = 1600; }}; buildConfigurationList = {cfg_list_proj} "
        f"/* Build configuration list for PBXProject \"Wilfred\" */; compatibilityVersion = "
        f"\"Xcode 14.0\"; developmentRegion = en; hasScannedForEncodings = 0; "
        f"knownRegions = (en, Base); mainGroup = {main_group}; productRefGroup = {products_group} "
        f"/* Products */; projectDirPath = \"\"; projectRoot = \"\"; targets = ({target} /* Wilfred */); }};")
    end("PBXProject")

    begin("PBXResourcesBuildPhase")
    res = ", ".join(
        f"{uuid_for('buildfile:' + p)} /* {os.path.basename(p)} in Resources */"
        for p, _, phase in entries
        if phase == "resources"
    )
    add(f"\t\t{phase_resources} /* Resources */ = {{isa = PBXResourcesBuildPhase; "
        f"buildActionMask = 2147483647; files = ({res},); runOnlyForDeploymentPostprocessing = 0; }};")
    end("PBXResourcesBuildPhase")

    begin("PBXSourcesBuildPhase")
    src = ", ".join(
        f"{uuid_for('buildfile:' + p)} /* {os.path.basename(p)} in Sources */"
        for p, _, phase in entries
        if phase == "sources"
    )
    add(f"\t\t{phase_sources} /* Sources */ = {{isa = PBXSourcesBuildPhase; "
        f"buildActionMask = 2147483647; files = ({src},); runOnlyForDeploymentPostprocessing = 0; }};")
    end("PBXSourcesBuildPhase")

    # Configurations
    def xcconfig(uid: str, name: str, settings: "dict[str, str]") -> None:
        add(f"\t\t{uid} /* {name} */ = {{isa = XCBuildConfiguration; buildSettings = {{")
        for k in sorted(settings):
            add(f"\t\t\t{k} = {settings[k]};")
        add(f"\t\t}}; name = {name}; }};")

    begin("XCBuildConfiguration")
    xcconfig(cfg_proj_dbg, "Debug", {"SDKROOT": "iphoneos"})
    xcconfig(cfg_proj_rel, "Release", {"SDKROOT": "iphoneos"})
    base_tgt = {
        "ALWAYS_SEARCH_USER_PATHS": "NO",
        "ASSETCATALOG_COMPILER_APPICON_NAME": "AppIcon",
        "ASSETCATALOG_COMPILER_GLOBAL_ACCENT_COLOR_NAME": "AccentColor",
        "CLANG_CXX_LANGUAGE_STANDARD": '"c++20"',
        "CLANG_CXX_LIBRARY": '"libc++"',
        "CODE_SIGN_STYLE": "Automatic",
        "HEADER_SEARCH_PATHS": '"$(SRCROOT)/../include"',
        "INFOPLIST_FILE": '"Wilfred/Info.plist"',
        "IPHONEOS_DEPLOYMENT_TARGET": "17.0",
        "LD_RUNPATH_SEARCH_PATHS": '"$(inherited) @executable_path/Frameworks"',
        "MARKETING_VERSION": "1.0.0",
        "PRODUCT_BUNDLE_IDENTIFIER": "com.wilfred.launcher",
        "PRODUCT_NAME": '"$(TARGET_NAME)"',
        "SWIFT_OBJC_BRIDGING_HEADER": '"Wilfred/Wilfred-Bridging-Header.h"',
        "SWIFT_VERSION": "5.0",
        "TARGETED_DEVICE_FAMILY": '"1,2"',
    }
    dbg = dict(base_tgt)
    dbg.update(
        {
            "CURRENT_PROJECT_VERSION": "1",
            "DEBUG_INFORMATION_FORMAT": "dwarf",
            "ENABLE_TESTABILITY": "YES",
            "GCC_OPTIMIZATION_LEVEL": "0",
            "ONLY_ACTIVE_ARCH": "YES",
            "SWIFT_OPTIMIZATION_LEVEL": '"-Onone"',
        }
    )
    rel = dict(base_tgt)
    rel.update(
        {
            "CURRENT_PROJECT_VERSION": "1",
            "DEBUG_INFORMATION_FORMAT": '"dwarf-with-dsym"',
            "GCC_OPTIMIZATION_LEVEL": "s",
            "SWIFT_OPTIMIZATION_LEVEL": '"-O"',
        }
    )
    xcconfig(cfg_tgt_dbg, "Debug", dbg)
    xcconfig(cfg_tgt_rel, "Release", rel)
    end("XCBuildConfiguration")

    begin("XCConfigurationList")
    add(f"\t\t{cfg_list_proj} /* Build configuration list for PBXProject \"Wilfred\" */ = "
        f"{{isa = XCConfigurationList; buildConfigurations = ({cfg_proj_dbg} /* Debug */, "
        f"{cfg_proj_rel} /* Release */); defaultConfigurationIsVisible = 0; "
        f"defaultConfigurationName = Release; }};")
    add(f"\t\t{cfg_list_target} /* Build configuration list for PBXNativeTarget \"Wilfred\" */ = "
        f"{{isa = XCConfigurationList; buildConfigurations = ({cfg_tgt_dbg} /* Debug */, "
        f"{cfg_tgt_rel} /* Release */); defaultConfigurationIsVisible = 0; "
        f"defaultConfigurationName = Release; }};")
    end("XCConfigurationList")

    add("\t};")
    add(f"\trootObject = {proj} /* Project object */;")
    add("}")

    if check_only:
        print(f"ios project OK: {len(core)} core + {len(backend)} ios backend + "
              f"{len(app)} app sources, all on disk")
        return 0
    os.makedirs(PROJ_DIR, exist_ok=True)
    with open(PBXPROJ, "w", encoding="utf-8", newline="\n") as f:
        f.write("\n".join(lines) + "\n")

    # Shared scheme (required: xcodebuild demands -scheme/-xctestrun with
    # -derivedDataPath; scripts/build-ios.sh builds -scheme Wilfred).
    scheme_dir = os.path.join(PROJ_DIR, "xcshareddata", "xcschemes")
    os.makedirs(scheme_dir, exist_ok=True)
    with open(os.path.join(scheme_dir, "Wilfred.xcscheme"), "w", encoding="utf-8",
              newline="\n") as f:
        f.write(f"""<?xml version="1.0" encoding="UTF-8"?>
<Scheme
   LastUpgradeVersion = "1600"
   version = "1.7">
   <BuildAction
      parallelizeBuildables = "YES"
      buildImplicitDependencies = "YES"
      runPostActionsOnFailure = "NO">
      <BuildActionEntries>
         <BuildActionEntry
            buildForTesting = "YES"
            buildForRunning = "YES"
            buildForProfiling = "YES"
            buildForArchiving = "YES"
            buildForAnalyzing = "YES">
            <BuildableReference
               BuildableIdentifier = "primary"
               BlueprintIdentifier = "{target}"
               BuildableName = "Wilfred.app"
               BlueprintName = "Wilfred"
               ReferencedContainer = "container:Wilfred.xcodeproj">
            </BuildableReference>
         </BuildActionEntry>
      </BuildActionEntries>
   </BuildAction>
   <LaunchAction
      buildConfiguration = "Debug"
      selectedDebuggerIdentifier = "Xcode.DebuggerFoundation.Debugger.LLDB"
      selectedLauncherIdentifier = "Xcode.DebuggerFoundation.Launcher.LLDB"
      launchStyle = "0"
      useCustomWorkingDirectory = "NO"
      ignoresPersistentStateOnLaunch = "NO"
      debugDocumentVersioning = "YES"
      debugServiceExtension = "internal"
      allowLocationSimulation = "YES">
      <BuildableProductRunnable
         runnableDebuggingMode = "0">
         <BuildableReference
            BuildableIdentifier = "primary"
            BlueprintIdentifier = "{target}"
            BuildableName = "Wilfred.app"
            BlueprintName = "Wilfred"
            ReferencedContainer = "container:Wilfred.xcodeproj">
         </BuildableReference>
      </BuildableProductRunnable>
   </LaunchAction>
   <ProfileAction
      buildConfiguration = "Release"
      shouldUseLaunchSchemeArgsEnv = "YES"
      savedToolIdentifier = ""
      useCustomWorkingDirectory = "NO"
      debugDocumentVersioning = "YES">
      <BuildableProductRunnable
         runnableDebuggingMode = "0">
         <BuildableReference
            BuildableIdentifier = "primary"
            BlueprintIdentifier = "{target}"
            BuildableName = "Wilfred.app"
            BlueprintName = "Wilfred"
            ReferencedContainer = "container:Wilfred.xcodeproj">
         </BuildableReference>
      </BuildableProductRunnable>
   </ProfileAction>
</Scheme>
""")
    print(f"wrote {PBXPROJ} ({len(core)} core + {len(backend)} ios + {len(app)} app)")
    return 0


if __name__ == "__main__":
    sys.exit(main())
