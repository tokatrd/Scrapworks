#!/bin/bash
# Deploy MCP server stack to 192.168.0.2
# Usage: ./deploy.sh [user@host]
#   ./deploy.sh                              # prompts for host
#   ./deploy.sh toka@192.168.0.2             # deploy to server
#   ./deploy.sh --local                      # deploy locally for testing

set -e

HOST="${1:-}"
DIR="$(cd "$(dirname "$0")" && pwd)"
STACK_DIR="/home/toka/mcp-stack"

deploy_local() {
  echo "=== Deploying locally ==="
  cd "$DIR"
  docker compose pull
  docker compose up -d
  echo "=== Local deploy complete ==="
  echo "MCPHub: http://localhost:8090"
  echo "DuckDuckGo: http://localhost:8020"
  echo "Docker MCP: http://localhost:8021"
  echo "Fetch MCP: http://localhost:8022"
  echo "Netdata: http://localhost:19999"
  echo "ntfy: http://localhost:8023"
  echo "Filesystem: http://localhost:8024"
  echo "SQLite: http://localhost:8025"
}

deploy_remote() {
  local host="$1"
  echo "=== Deploying to $host ==="
  
  # Copy stack files to server
  echo "Copying files..."
  ssh "$host" "mkdir -p $STACK_DIR"
  scp -r "$DIR"/* "$host:$STACK_DIR/"
  
  # Start the stack
  echo "Starting Docker stack..."
  ssh "$host" "cd $STACK_DIR && docker compose pull && docker compose up -d"
  
  echo "=== Remote deploy complete ==="
  echo "Services running on $host:"
  echo "  MCPHub:     http://$host:8090"
  echo "  DuckDuckGo: http://$host:8020"
  echo "  Docker MCP: http://$host:8021"
  echo "  Fetch MCP:  http://$host:8022"
  echo "  Netdata:    http://$host:19999"
  echo "  ntfy:       http://$host:8023"
  echo "  Filesystem: http://$host:8024"
  echo "  SQLite:     http://$host:8025"
  echo ""
  echo "=== Next steps ==="
  echo "1. Add proxy hosts in Nginx Proxy Manager (port 8181)"
  echo "2. Add services to Homepage dashboard (home.peralezf.com)"
  echo "3. Update opencode MCP config"
}

if [ "$HOST" = "--local" ]; then
  deploy_local
elif [ -z "$HOST" ]; then
  echo "Usage: $0 [user@host | --local]"
  echo "  --local    deploy on this machine for testing"
  echo "  user@host  deploy to remote server (e.g., toka@192.168.0.2)"
  exit 1
else
  deploy_remote "$HOST"
fi
