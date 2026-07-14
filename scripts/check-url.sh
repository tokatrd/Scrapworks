#!/bin/bash
# check-url.sh - Check HTTP URL health with timing and status
# Usage: check-url.sh <url> [--timeout N]

URL="$1"
TIMEOUT="${2:-5}"

[ -z "$URL" ] && echo "Usage: check-url.sh <url> [--timeout N]" >&2 && exit 1

START=$(date +%s%N)
STATUS=$(curl -s -o /dev/null -w "%{http_code}" --max-time "$TIMEOUT" "$URL" 2>/dev/null)
END=$(date +%s%N)
MS=$(( (END - START) / 1000000 ))

if [ -z "$STATUS" ] || [ "$STATUS" = "000" ]; then
  echo "FAIL: $URL (timeout or unreachable after ${TIMEOUT}s)"
  exit 1
fi

case "$STATUS" in
  2??) echo "OK:   $URL -> $STATUS (${MS}ms)" ;;
  3??) echo "MOVED: $URL -> $STATUS (${MS}ms)" ;;
  4??) echo "FAIL: $URL -> $STATUS (${MS}ms)" ;;
  5??) echo "ERROR: $URL -> $STATUS (${MS}ms)" ;;
  *)   echo "UNKNOWN: $URL -> $STATUS (${MS}ms)" ;;
esac
