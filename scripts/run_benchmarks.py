#!/usr/bin/env python3
"""
Wilfred Benchmark Runner

Runs the wilfred_bench executable with various configurations and outputs
results in multiple formats (console, JSON, Markdown, CSV).
"""

import argparse
import json
import subprocess
import sys
import os
import platform
import time
from pathlib import Path
from typing import List, Dict, Any, Optional

def get_build_type() -> str:
    """Get the build type from CMake cache or environment."""
    build_type = os.environ.get("CMAKE_BUILD_TYPE", "Release")
    return build_type

def get_bench_executable(build_dir: Path) -> Path:
    """Find the wilfred_bench executable."""
    candidates = [
        build_dir / "wilfred_bench",
        build_dir / "Release" / "wilfred_bench.exe",
        build_dir / "Debug" / "wilfred_bench.exe",
        build_dir / "bench" / "wilfred_bench",
    ]
    for c in candidates:
        if c.exists():
            return c
    raise FileNotFoundError(f"wilfred_bench not found in {build_dir}")

def run_bench(executable: Path, extra_args: List[str] = None) -> str:
    """Run the benchmark and return stdout."""
    cmd = [str(executable)]
    if extra_args:
        cmd.extend(extra_args)
    print(f"Running: {' '.join(cmd)}")
    result = subprocess.run(cmd, capture_output=True, text=True, timeout=300)
    if result.returncode != 0:
        print(f"Benchmark failed with code {result.returncode}")
        print(f"stderr: {result.stderr}")
        raise RuntimeError("Benchmark execution failed")
    return result.stdout

def parse_bench_output(output: str) -> List[Dict[str, Any]]:
    """Parse the benchmark output table into structured data."""
    lines = output.strip().split('\n')
    results = []
    in_table = False
    for line in lines:
        if line.startswith("Benchmark"):
            in_table = True
            continue
        if in_table and line.startswith("-"):
            continue
        if in_table and line.strip():
            parts = line.split()
            if len(parts) >= 6:
                # Benchmark name may contain spaces, so join all but last 5
                name = " ".join(parts[:-5])
                try:
                    results.append({
                        "name": name,
                        "min_ms": float(parts[-5]),
                        "median_ms": float(parts[-4]),
                        "mean_ms": float(parts[-3]),
                        "p95_ms": float(parts[-2]),
                        "max_ms": float(parts[-1]),
                    })
                except ValueError:
                    pass
    return results

def format_markdown_table(results: List[Dict[str, Any]], title: str) -> str:
    """Format results as a Markdown table."""
    md = [f"## {title}", ""]
    md.append("| Benchmark | Min (ms) | Median (ms) | Mean (ms) | P95 (ms) | Max (ms) |")
    md.append("|---|---:|---:|---:|---:|---:|")
    for r in results:
        md.append(f"| {r['name']} | {r['min_ms']:.3f} | {r['median_ms']:.3f} | {r['mean_ms']:.3f} | {r['p95_ms']:.3f} | {r['max_ms']:.3f} |")
    md.append("")
    return "\n".join(md)

def format_json(results: List[Dict[str, Any]], metadata: Dict[str, Any]) -> str:
    """Format results as JSON."""
    data = {
        "metadata": metadata,
        "results": results,
    }
    return json.dumps(data, indent=2)

def format_csv(results: List[Dict[str, Any]]) -> str:
    """Format results as CSV."""
    lines = ["benchmark,min_ms,median_ms,mean_ms,p95_ms,max_ms"]
    for r in results:
        lines.append(f"{r['name']},{r['min_ms']:.3f},{r['median_ms']:.3f},{r['mean_ms']:.3f},{r['p95_ms']:.3f},{r['max_ms']:.3f}")
    return "\n".join(lines)

def get_system_info() -> Dict[str, Any]:
    """Collect system information for benchmark metadata."""
    import cpuinfo
    try:
        cpu = cpuinfo.get_cpu_info()
        cpu_name = cpu.get("brand_raw", platform.processor())
    except Exception:
        cpu_name = platform.processor()
    return {
        "timestamp": time.strftime("%Y-%m-%dT%H:%M:%SZ", time.gmtime()),
        "platform": platform.system(),
        "platform_version": platform.version(),
        "architecture": platform.machine(),
        "cpu": cpu_name,
        "cpu_count": os.cpu_count(),
        "python_version": platform.python_version(),
        "build_type": get_build_type(),
    }

def main():
    parser = argparse.ArgumentParser(description="Run Wilfred benchmarks and publish results")
    parser.add_argument("--build-dir", type=Path, default=Path("build"),
                        help="Build directory containing wilfred_bench")
    parser.add_argument("--output", type=Path, help="Output file (default: stdout)")
    parser.add_argument("--format", choices=["console", "markdown", "json", "csv", "all"],
                        default="console", help="Output format")
    parser.add_argument("--extra-args", nargs="*", help="Extra arguments to pass to wilfred_bench")
    parser.add_argument("--metadata", type=Path, help="JSON file with additional metadata")
    parser.add_argument("--publish", action="store_true",
                        help="Publish results to GitHub (requires gh CLI)")
    parser.add_argument("--repo", default="ajakfial/wilfred",
                        help="GitHub repository for publishing")
    args = parser.parse_args()

    # Find executable
    try:
        bench_exe = get_bench_executable(args.build_dir)
    except FileNotFoundError as e:
        print(f"Error: {e}")
        sys.exit(1)

    print(f"Using benchmark executable: {bench_exe}")

    # Run benchmark
    output = run_bench(bench_exe, args.extra_args)

    # Parse results
    results = parse_bench_output(output)

    # Collect metadata
    metadata = get_system_info()
    if args.metadata and args.metadata.exists():
        with open(args.metadata) as f:
            metadata.update(json.load(f))
    metadata["build_dir"] = str(args.build_dir.absolute())
    metadata["executable"] = str(bench_exe.absolute())

    # Format output
    if args.format == "console":
        formatted = output
    elif args.format == "markdown":
        formatted = format_markdown_table(results, "Wilfred Benchmark Results")
    elif args.format == "json":
        formatted = format_json(results, metadata)
    elif args.format == "csv":
        formatted = format_csv(results)
    elif args.format == "all":
        # Output all formats to separate files
        base = args.output or Path("benchmark_results")
        base.parent.mkdir(parents=True, exist_ok=True)
        (base.with_suffix(".md")).write_text(format_markdown_table(results, "Wilfred Benchmark Results"))
        (base.with_suffix(".json")).write_text(format_json(results, metadata))
        (base.with_suffix(".csv")).write_text(format_csv(results))
        print(f"Results written to {base.with_suffix('.md')}, {base.with_suffix('.json')}, {base.with_suffix('.csv')}")
        return
    else:
        formatted = output

    # Write output
    if args.output:
        args.output.parent.mkdir(parents=True, exist_ok=True)
        args.output.write_text(formatted)
        print(f"Results written to {args.output}")
    else:
        print(formatted)

    # Publish if requested
    if args.publish:
        if args.format != "markdown":
            print("Publishing requires markdown format")
            sys.exit(1)
        # Create a gist or comment on a PR/issue
        # This is a placeholder for actual publishing logic
        print("Publishing not yet implemented - use 'gh gist create' manually")

if __name__ == "__main__":
    main()