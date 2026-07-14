#!/bin/bash
# extract.sh - Extract lines matching a pattern from file or stdin
# Usage: extract.sh <pattern> [file]
#        command | extract.sh <pattern>
#        extract.sh -r <regex> [file]   # extended regex mode

if [ "$1" = "-r" ]; then
  shift
  PATTERN="$1"
  shift
else
  PATTERN="$1"
  shift
fi

[ -z "$PATTERN" ] && echo "Usage: extract.sh <pattern> [file]" >&2 && exit 1

if [ -n "$1" ]; then
  grep -n -- "$PATTERN" "$1" 2>/dev/null || echo "No matches or file not found" >&2
else
  grep -n -- "$PATTERN" /dev/stdin
fi
