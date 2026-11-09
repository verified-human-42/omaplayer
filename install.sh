#!/bin/bash
# Installs Omaplayer on Omarchy (or any Arch system) as a regular pacman
# package. It takes the prebuilt package from the latest release, so there's
# nothing to compile:
#
#   curl -fsSL https://raw.githubusercontent.com/verified-human-42/omaplayer/main/install.sh | bash
#
# Options, after `bash -s --`:
#   --build   compile the latest release here instead of downloading it
#   --git     compile the latest from main
#
# Run it again to update. Remove with: sudo pacman -R omaplayer
set -euo pipefail

repo=verified-human-42/omaplayer
mode=prebuilt
case ${1:-} in
--build) mode=build ;;
--git) mode=git ;;
esac

if [[ $EUID -eq 0 ]]; then
  echo "Run this as your own user; it asks for sudo when it installs." >&2
  exit 1
fi
if ! command -v pacman >/dev/null; then
  echo "Omaplayer installs as a pacman package, on Omarchy or Arch." >&2
  exit 1
fi

# The other flavour, if installed, makes way for this one.
make_way_for() {
  local other=omaplayer-git
  [[ $1 == omaplayer-git ]] && other=omaplayer
  if pacman -Q "$other" >/dev/null 2>&1; then
    sudo pacman -R --noconfirm "$other"
  fi
}

done_message() {
  echo
  echo "Omaplayer is installed. Find it in the app launcher (Super + Space), or run: omaplayer"
}

# The prebuilt package attached to the latest release. pacman pulls in mpv,
# Qt, and anything else it needs from the Arch repositories.
if [[ $mode == prebuilt ]]; then
  arch=$(uname -m)
  url=$(curl -fsSL "https://api.github.com/repos/$repo/releases/latest" |
    grep -o "\"browser_download_url\": *\"[^\"]*/omaplayer-[0-9][^\"]*-$arch\.pkg\.tar\.zst\"" |
    grep -v -- '-debug-' | head -n1 | sed 's/.*"\(https[^"]*\)"$/\1/') || true
  if [[ -n $url ]]; then
    make_way_for omaplayer
    sudo pacman -U --needed --noconfirm "$url"
    done_message
    exit 0
  fi
  echo "No prebuilt package for $arch in the latest release; building it instead."
  mode=build
fi

# Building: makepkg and git, if this machine has never built a package.
package=omaplayer
[[ $mode == git ]] && package=omaplayer-git
sudo pacman -S --needed --noconfirm base-devel git

work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT
curl -fsSL "https://raw.githubusercontent.com/$repo/main/packaging/$package/PKGBUILD" -o "$work/PKGBUILD"
make_way_for "$package"

cd "$work"
# -s installs what's needed to build, -i installs the result; the tests ran
# when the release was made.
makepkg -si --noconfirm --nocheck
done_message
