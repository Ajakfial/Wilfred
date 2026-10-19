#!/usr/bin/env python3
"""
Scaffold a new test file for Wilfred.
Usage:
  python3 .agents/scripts/add-test.py --name test_foo --file tests/test_foo.cpp
"""
import argparse
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
TESTS_DIR = ROOT / "tests"

TEMPLATE = '''#include "test.hpp"

#include "wilfred/{header}"

void {name}() {{
    using namespace wilfred;

    // Arrange
    {arrange}

    // Act
    {act}

    // Assert
    {assert}
}}
'''

def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--name", required=True, help="Test function name (e.g., test_my_feature)")
    parser.add_argument("--file", required=True, help="Output file path (e.g., tests/test_foo.cpp)")
    parser.add_argument("--header", help="Header to include (e.g., search/engine.hpp)")
    parser.add_argument("--arrange", default="// TODO: setup")
    parser.add_argument("--act", default="// TODO: call function")
    parser.add_argument("--assert", default="CHECK_EQ(result, expected)", dest="assert_")
    args = parser.parse_args()

    output = ROOT / args.file
    if output.exists():
        print(f"Error: {output} already exists")
        return 1

    header = args.header or f"search/{args.name.replace('test_', '')}.hpp"

    content = TEMPLATE.format(
        name=args.name,
        header=header,
        arrange=args.arrange,
        act=args.act,
        assert=args.assert_
    )

    output.parent.mkdir(parents=True, exist_ok=True)
    output.write_text(content)
    print(f"Created {output.relative_to(ROOT)}")
    print("Edit the TODO sections, then run ./build/wilfred_tests")
    return 0

if __name__ == "__main__":
    import sys
    sys.exit(main())