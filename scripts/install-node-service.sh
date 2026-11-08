#!/usr/bin/env bash
# Install (or refresh) the MeetAgent Node as a systemd user service.
#
# The service restarts automatically and reads OPENCODE_PASSWORD from
# ~/.config/meetagent/node.env. If OPENCODE_PASSWORD is not set in the current
# environment, the script asks the local OpenCode service for its password.
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
UNIT_SRC="$ROOT/deploy/systemd/meetagent-node.service"
UNIT_DIR="$HOME/.config/systemd/user"
UNIT_DST="$UNIT_DIR/meetagent-node.service"
ENV_DIR="$HOME/.config/meetagent"
ENV_FILE="$ENV_DIR/node.env"

if [[ ! -x "$ROOT/bin/meetagent-node" ]]; then
  echo "building bin/meetagent-node..."
  (cd "$ROOT" && make node)
fi

mkdir -p "$ENV_DIR" "$UNIT_DIR"
chmod 700 "$ENV_DIR"
if [[ ! -f "$ENV_FILE" ]]; then
  password="${OPENCODE_PASSWORD:-}"
  if [[ -z "$password" ]] && command -v opencode >/dev/null 2>&1; then
    password="$(opencode service get password 2>/dev/null || true)"
  fi
  if [[ -z "$password" ]]; then
    echo "OPENCODE_PASSWORD is not set and 'opencode service get password' failed." >&2
    echo "Export OPENCODE_PASSWORD and rerun, or create $ENV_FILE manually." >&2
    exit 1
  fi
  umask 077
  printf 'OPENCODE_PASSWORD=%s\n' "$password" >"$ENV_FILE"
fi
chmod 600 "$ENV_FILE"

sed "s|@ROOT@|$ROOT|g" "$UNIT_SRC" >"$UNIT_DST"
systemctl --user daemon-reload
systemctl --user enable --now meetagent-node.service

if command -v loginctl >/dev/null 2>&1; then
  if [[ "$(loginctl show-user "$USER" -p Linger 2>/dev/null || true)" != "Linger=yes" ]]; then
    loginctl enable-linger "$USER" >/dev/null 2>&1 || \
      echo "warning: could not enable linger; run: sudo loginctl enable-linger $USER" >&2
  fi
fi

systemctl --user --no-pager --lines=8 status meetagent-node.service || true
echo
echo "logs: journalctl --user -u meetagent-node -f"
