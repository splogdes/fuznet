#!/usr/bin/env bash
set -euo pipefail

# -------------------------------------------------------------------
# install_user_service.sh
# Installs & starts fuznet.service as a systemd --user unit.
# No sudo required. Warns if an old unit is present.
# -------------------------------------------------------------------

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"

SERVICE_DIR="$HOME/.config/systemd/user"
SERVICE_FILE="$SERVICE_DIR/fuznet.service"

if [[ -f "$SERVICE_FILE" ]]; then
    echo "⚠️  Warning: existing service file at $SERVICE_FILE will be overwritten"
fi

mkdir -p "$SERVICE_DIR"

cat > "$SERVICE_FILE" <<EOF
[Unit]
Description=Fuznet endless Vivado-fuzz equivalence loop

[Service]
Type=simple
WorkingDirectory=$PROJECT_ROOT
ExecStart=/usr/bin/env bash $PROJECT_ROOT/scripts/fuzz_loop.sh
Restart=always
RestartSec=10
Nice=10

[Install]
WantedBy=default.target
EOF

echo "✔️  Wrote service unit to $SERVICE_FILE"


echo "⟳ Reloading user systemd units..."
systemctl --user daemon-reload

echo "⎈ Enabling fuznet.service for automatic start..."
systemctl --user enable fuznet.service

echo "▶️  Starting fuznet.service now..."
systemctl --user restart fuznet.service

echo "✅ fuznet.service is running under your user. Check with:"
echo "    systemctl --user status fuznet.service"
echo "    journalctl --user -u fuznet.service -f"
echo ""
echo "To stop the service, run:"
echo "    systemctl --user stop fuznet.service"
echo ""
echo "To disable the service, run:"
echo "    systemctl --user disable fuznet.service"
echo ""
echo "To uninstall the service, run:"
echo "    rm $SERVICE_FILE"
