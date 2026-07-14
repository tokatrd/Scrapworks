# Homepage Dashboard Integration

After deploying the MCP stack, add each service to the Homepage dashboard
at home.peralezf.com by editing Homepage's YAML config.

## Docker Compose Integration

Homepage auto-discovers Docker containers. Tag MCP containers in their labels:

```yaml
services:
  duckduckgo-mcp:
    labels:
      - "homepage.group=MCP Servers"
      - "homepage.name=DuckDuckGo Search"
      - "homepage.icon=search"
      - "homepage.href=http://192.168.0.2:8020"
      - "homepage.description=Privacy web search (no API key)"

  docker-mcp:
    labels:
      - "homepage.group=MCP Servers"
      - "homepage.name=Docker MCP"
      - "homepage.icon=docker"
      - "homepage.href=http://192.168.0.2:8021"
      - "homepage.description=Container and image management"

  fetch-mcp:
    labels:
      - "homepage.group=MCP Servers"
      - "homepage.name=Fetch MCP"
      - "homepage.icon=globe"
      - "homepage.href=http://192.168.0.2:8022"
      - "homepage.description=Web URL fetching and scraping"

  ntfy:
    labels:
      - "homepage.group=MCP Servers"
      - "homepage.name=ntfy Notifications"
      - "homepage.icon=bell"
      - "homepage.href=http://192.168.0.2:8023"
      - "homepage.description=Push notifications and alerts"

  filesystem-mcp:
    labels:
      - "homepage.group=MCP Servers"
      - "homepage.name=Filesystem MCP"
      - "homepage.icon=folder"
      - "homepage.href=http://192.168.0.2:8024"
      - "homepage.description=Filesystem access (sandboxed)"

  sqlite-mcp:
    labels:
      - "homepage.group=MCP Servers"
      - "homepage.name=SQLite MCP"
      - "homepage.icon=database"
      - "homepage.href=http://192.168.0.2:8025"
      - "homepage.description=SQLite database management"

  netdata:
    labels:
      - "homepage.group=Monitoring"
      - "homepage.name=Netdata"
      - "homepage.icon=chart-line"
      - "homepage.href=http://192.168.0.2:19999"
      - "homepage.description=Real-time system monitoring"

  mcphub:
    labels:
      - "homepage.group=MCP Servers"
      - "homepage.name=MCPHub"
      - "homepage.icon=plug"
      - "homepage.href=http://192.168.0.2:8090"
      - "homepage.description=MCP server aggregator"
```

## Services YAML (Homepage Config)

Alternatively, add to Homepage's `services.yaml`:

```yaml
- MCP Servers:
    - DuckDuckGo Search:
        icon: search
        href: http://192.168.0.2:8020
        description: Privacy web search (no API key)
    - Docker MCP:
        icon: docker
        href: http://192.168.0.2:8021
        description: Container and image management
    - Fetch MCP:
        icon: globe
        href: http://192.168.0.2:8022
        description: Web URL fetching and scraping
    - ntfy:
        icon: bell
        href: http://192.168.0.2:8023
        description: Push notifications
    - Filesystem MCP:
        icon: folder
        href: http://192.168.0.2:8024
        description: Filesystem access
    - SQLite MCP:
        icon: database
        href: http://192.168.0.2:8025
        description: SQLite database management
    - MCPHub:
        icon: plug
        href: http://192.168.0.2:8090
        description: MCP aggregator

- Monitoring:
    - Netdata:
        icon: chart-line
        href: http://192.168.0.2:19999
        description: Real-time system metrics
```

## Nginx Proxy Manager

Each MCP service can also be exposed behind NPM with a subdomain:

| Subdomain | Upstream URL | Notes |
|-----------|-------------|-------|
| mcp-ddg.home.peralezf.com | http://192.168.0.2:8020 | DuckDuckGo search |
| mcp-docker.home.peralezf.com | http://192.168.0.2:8021 | Docker manager |
| mcp-fetch.home.peralezf.com | http://192.168.0.2:8022 | URL fetcher |
| mcp-ntfy.home.peralezf.com | http://192.168.0.2:8023 | Notifications |
| mcp-fs.home.peralezf.com | http://192.168.0.2:8024 | Filesystem |
| mcp-sqlite.home.peralezf.com | http://192.168.0.2:8025 | SQLite |
| mcp-hub.home.peralezf.com | http://192.168.0.2:8090 | MCP aggregator |
| netdata.home.peralezf.com | http://192.168.0.2:19999 | System monitoring |

These subdomains would then be accessible via Cloudflare/Authentik SSO.
