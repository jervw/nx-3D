#!/usr/bin/env bash
# Host the nx-3D game server from this machine and print the address
# other players should use to connect.
#
# Usage:  ./scripts/host_server.sh
# Press Ctrl-C in the terminal running this script to stop the server.
set -euo pipefail

cd "$(dirname "${BASH_SOURCE[0]}")/.."

echo "=== Hosting nx-3D game server ==="
echo "Server ports: TCP 54200 (authentication), TCP 54202 (session), UDP 54201 (gameplay)"

# LAN address: first non-docker interface holding a global IPv4 address.
LAN_IP="$(ip -4 -o addr show scope global 2>/dev/null | grep -vE 'docker|br-' | awk '{print $4; exit}' | cut -d/ -f1)"
# Public address, when the machine can reach the internet.
PUBLIC_IP="$(curl -s --max-time 8 https://api.ipify.org 2>/dev/null || true)"

if [ -n "${LAN_IP:-}" ]; then
    echo "Your LAN IP:      ${LAN_IP}"
else
    echo "Your LAN IP:      (not found on this network)"
fi

if [ -n "${PUBLIC_IP:-}" ]; then
    echo "Your public IP:   ${PUBLIC_IP}"
    echo "Note: friends on the internet can only reach the public IP if the router"
    echo "      forwards TCP 54200, TCP 54202, and UDP 54201."
fi

echo
echo "Connect from this network with:"
echo "  ./build/client/nx_3D_client -server ${LAN_IP:-127.0.0.1} -username <name>"
echo
echo "Server passphrase: password"
echo

echo "Starting server (press Ctrl-C to stop)..."
exec ./build/server/nx-3D-server
