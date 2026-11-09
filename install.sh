#!/bin/bash
# Installs Omaplayer on Omarchy (or any Arch system) as a regular pacman
# package, built from the latest release:
#
#   curl -fsSL https://raw.githubusercontent.com/verified-human-42/omaplayer/main/install.sh | bash
#
# Pass --git for the latest from main instead:
#
#   curl -fsSL https://raw.githubusercontent.com/verified-human-42/omaplayer/main/install.sh | bash -s -- --git
#
# Run it again to update. Remove with: sudo pacman -R omaplayer
set -euo pipefail

repo=verified-human-42/omaplayer
package=omaplayer
[[ ${1:-} == --git ]] && package=omaplayer-git

if [[ $EUID -eq 0 ]]; then
  echo "Run this as your own user; it asks for sudo when it installs." >&2
  exit 1
fi
if ! command -v pacman >/dev/null; then
  echo "Omaplayer installs as a pacman package, on Omarchy or Arch." >&2
  exit 1
fi

# makepkg and git, if this machine has never built a package.
sudo pacman -S --needed --noconfirm base-devel git

work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT
curl -fsSL "https://raw.githubusercontent.com/$repo/main/packaging/$package/PKGBUILD" -o "$work/PKGBUILD"

# The other flavour, if installed, makes way for this one.
other=omaplayer-git
[[ $package == omaplayer-git ]] && other=omaplayer
if pacman -Q "$other" >/dev/null 2>&1; then
  sudo pacman -R --noconfirm "$other"
fi

cd "$work"
# -s installs what's needed to build, -i installs the result; the tests ran
# when the release was made.
makepkg -si --noconfirm --nocheck

echo
echo "Omaplayer is installed. Find it in the app launcher (Super + Space), or run: omaplayer"
