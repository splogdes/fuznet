#!/usr/bin/env bash

set -euo pipefail

# ──────────────── user‐overrideable ───────────────────────────────────────
BUILD_DIR=${BUILD_DIR:-build}
BUILD_TYPE=${BUILD_TYPE:-Debug}
GITHUB_REPO=${GITHUB_REPO:-splogdes/fuznet}
# ────────────────────────────────────────────────────────────────────────────

err()  { printf "\033[0;31m[ERROR]\033[0m %s\n" "$*" >&2; }
info() { printf "\033[0;34m[INFO] \033[0m%s\n" "$*"; }
pass() { printf "\033[0;32m[ OK ]\033[0m %s\n" "$*"; }


mkdir -p "$BUILD_DIR"
pushd "$BUILD_DIR" >/dev/null

info "Configuring (CMAKE_BUILD_TYPE=$BUILD_TYPE)…"
if cmake -DCMAKE_BUILD_TYPE="$BUILD_TYPE" .. \
   && make -j"$(nproc)"; then
  pass "Build succeeded"
  popd >/dev/null
  exit 0
fi

err "Build failed; attempting to fetch latest release binary"


for cmd in curl tar; do
  if ! command -v "$cmd" &>/dev/null; then
    err "Missing required tool: $cmd"
    exit 1
  fi
done


LATEST_TAG=$(curl -fsSL "https://api.github.com/repos/$GITHUB_REPO/releases/latest" \
             | awk -F'"' '/tag_name/ {print $4}')

if [[ -z "$LATEST_TAG" || "$LATEST_TAG" == "null" ]]; then
  err "Could not determine latest release tag"
  popd >/dev/null
  exit 1
fi

info "Latest release is $LATEST_TAG"


ASSET="fuznet-${LATEST_TAG}-linux.tar.gz"
URL="https://github.com/$GITHUB_REPO/releases/download/$LATEST_TAG/$ASSET"

info "Downloading $URL"
curl -fSL "$URL" -o "$ASSET"

info "Extracting $ASSET"
tar -xzf "$ASSET"
rm "$ASSET"

pass "Fetched and unpacked release $LATEST_TAG"
popd >/dev/null
exit 0