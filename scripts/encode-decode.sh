#!/bin/bash
# encode-decode.sh - Encode/decode various formats
# Usage: encode-decode.sh <command> [data]
# Commands: b64enc, b64dec, hexenc, hexdec, urlenc, urldec

CMD="$1"
DATA="$2"

[ -z "$CMD" ] && echo "Usage: encode-decode.sh <command> [data]" >&2 && echo "Commands: b64enc b64dec hexenc hexdec urlenc urldec" >&2 && exit 1

if [ -z "$DATA" ]; then
  DATA=$(cat /dev/stdin)
fi

case "$CMD" in
  b64enc) echo -n "$DATA" | base64 ;;
  b64dec) echo -n "$DATA" | base64 -d 2>/dev/null || echo "Invalid base64" >&2 ;;
  hexenc) echo -n "$DATA" | xxd -p ;;
  hexdec) echo -n "$DATA" | xxd -r -p 2>/dev/null || echo "Invalid hex" >&2 ;;
  urlenc) echo -n "$DATA" | python3 -c "import sys,urllib.parse; print(urllib.parse.quote(sys.stdin.read().strip()))" ;;
  urldec) echo -n "$DATA" | python3 -c "import sys,urllib.parse; print(urllib.parse.unquote(sys.stdin.read().strip()))" ;;
  *) echo "Unknown command: $CMD" >&2; exit 1 ;;
esac
