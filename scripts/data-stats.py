#!/usr/bin/env python3
"""data-stats.py - Quick statistics on CSV or JSON data.
Usage:
  ./data-stats.py data.csv              # column stats
  ./data-stats.py data.json             # key stats
  ./data-stats.py data.csv --col Name   # unique values in column
"""
import csv, json, sys, os
from collections import Counter
from pathlib import Path

def csv_stats(path):
    with open(path) as f:
        reader = csv.DictReader(f)
        rows = list(reader)
    print(f"Rows: {len(rows)}, Columns: {len(reader.fieldnames)}")
    print(f"Fields: {', '.join(reader.fieldnames)}")
    if "--col" in sys.argv:
        col = sys.argv[sys.argv.index("--col") + 1]
        vals = Counter(r[col] for r in rows if col in r)
        for val, count in vals.most_common(10):
            print(f"  {val}: {count}")

def json_stats(path):
    data = json.loads(Path(path).read_text())
    if isinstance(data, list):
        print(f"Array of {len(data)} items")
        if data and isinstance(data[0], dict):
            print(f"Keys: {', '.join(data[0].keys())}")
    elif isinstance(data, dict):
        print(f"Object with {len(data)} keys")
        print(f"Keys: {', '.join(list(data.keys())[:20])}")

if len(sys.argv) < 2:
    print("Usage: data-stats.py <file.csv|file.json> [--col <name>]")
    sys.exit(1)

path = sys.argv[1]
if path.endswith(".csv"):
    csv_stats(path)
elif path.endswith(".json"):
    json_stats(path)
else:
    print("Unsupported file type (use .csv or .json)")
