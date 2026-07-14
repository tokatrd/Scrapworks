#!/bin/bash
# template-gen.sh - Generate a file from a template with variable substitution
# Usage: template-gen.sh <template-file> [key=value ...]
#
# Template variables use {{VAR_NAME}} syntax.
# Example:
#   echo "Hello {{NAME}}" > /tmp/tpl.txt
#   template-gen.sh /tmp/tpl.txt NAME=World

TPL_FILE="$1"
shift

[ -z "$TPL_FILE" ] && echo "Usage: template-gen.sh <template-file> [key=value ...]" >&2 && exit 1
[ ! -f "$TPL_FILE" ] && echo "Template not found: $TPL_FILE" >&2 && exit 1

content=$(cat "$TPL_FILE")

for pair in "$@"; do
  key="${pair%%=*}"
  val="${pair#*=}"
  content="${content//\{\{$key\}\}/$val}"
done

echo "$content"
