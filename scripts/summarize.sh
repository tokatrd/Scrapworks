#!/bin/bash
# summarize.sh - Summarize text from a file or stdin
# Usage: summarize.sh [file]          # summarize a file
#        command | summarize.sh       # summarize piped input
#        summarize.sh [file] --lines N # limit summary to N lines

LINES="${2:-10}"
INPUT_FILE="$1"

if [ -n "$INPUT_FILE" ]; then
  if [ ! -f "$INPUT_FILE" ]; then echo "File not found: $INPUT_FILE" >&2; exit 1; fi
  head -n "$LINES" "$INPUT_FILE" | nl -ba
  echo "..."
  wc -l "$INPUT_FILE" | awk '{print "Total: " $1 " lines"}'
else
  head -n "$LINES" /dev/stdin | nl -ba
  lines=$(cat /dev/stdin 2>/dev/null | wc -l)
  [ "$lines" -gt 0 ] && echo "Total: $lines lines" || true
fi
