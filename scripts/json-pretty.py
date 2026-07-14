#!/usr/bin/env python3
"""json-pretty.py - Pretty-print, validate, or extract from JSON.
Usage:
  ./json-pretty.py < file.json         # pretty-print
  ./json-pretty.py file.json --key path.to.key  # extract nested key
  command | ./json-pretty.py --minify   # minify to one line
"""
import json, sys, os
from pathlib import Path

def get_key(obj, path):
    parts = path.split(".")
    for p in parts:
        if isinstance(obj, dict):
            obj = obj[p]
        elif isinstance(obj, list):
            obj = obj[int(p)]
        else:
            raise KeyError(p)
    return obj

def main():
    data = None
    if len(sys.argv) > 1 and not sys.argv[1].startswith("--"):
        path = Path(sys.argv[1])
        if path.exists():
            data = json.loads(path.read_text())
            sys.argv.pop(1)
    if data is None:
        data = json.loads(sys.stdin.read())

    if "--key" in sys.argv:
        idx = sys.argv.index("--key")
        data = get_key(data, sys.argv[idx + 1])

    if "--minify" in sys.argv:
        json.dump(data, sys.stdout, separators=(",", ":"))
    else:
        json.dump(data, sys.stdout, indent=2)
    print()

if __name__ == "__main__":
    main()
