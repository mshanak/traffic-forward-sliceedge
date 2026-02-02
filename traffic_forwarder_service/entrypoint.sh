#!/bin/sh
set -eu

# If the user provided arguments to `docker run`, use them directly:
#   docker run image --bind 0.0.0.0 udp:5000=1.2.3.4:5000
if [ "$#" -gt 0 ]; then
  exec ./port_service_forwarder "$@"
fi

# Otherwise, compose arguments from env vars.
BIND_IP="${BIND_IP:-0.0.0.0}"
MAPPINGS="${MAPPINGS:-}"

# Fallback mapping if none provided
if [ -z "$MAPPINGS" ]; then
  MAPPINGS="tcp:8080=127.0.0.1:80"
fi

# Build argv: rely on standard word splitting of MAPPINGS (space-separated items)
# e.g. MAPPINGS="udp:5000=1.2.3.4:5000 udp:6000=1.2.3.5:6000"
# shellcheck disable=SC2086
exec ./port_service_forwarder --bind "$BIND_IP" $MAPPINGS
