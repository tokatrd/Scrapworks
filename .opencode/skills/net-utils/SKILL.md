---
name: net-utils
description: Network monitoring and URL checking utilities. Use when the user asks to check if a URL is reachable, test connection, or measure response time.
---

# Network Utilities

Scripts for checking network endpoints.

## Available Tools

### check-url.sh
Check HTTP URL health with status code and timing.
```
scripts/check-url.sh https://example.com
scripts/check-url.sh https://slow.site --timeout 10
```

## Guidelines
- Reports: OK (2xx), MOVED (3xx), FAIL (4xx), ERROR (5xx)
- Includes response time in milliseconds
- Default timeout is 5 seconds
