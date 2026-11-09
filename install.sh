#!/bin/bash
# Installs OmaPlayer on Omarchy (or any Arch system) as a regular pacman
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
  echo "OmaPlayer installs as a pacman package, on Omarchy or Arch." >&2
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

# omarchy-update runs the scripts in this folder when it's done, so OmaPlayer
# updates along with everything else. It checks the latest release and only
# downloads when there's a newer one.
install_update_hook() {
  local hooks="$HOME/.config/omarchy/hooks/post-update.d"
  mkdir -p "$hooks"
  cat >"$hooks/omaplayer" <<'HOOK'
#!/bin/bash
# Installed by OmaPlayer: omarchy-update updates OmaPlayer too.
# Remove this file to stop it.
installed=$(pacman -Q omaplayer 2>/dev/null | awk '{print $2}')
[[ -n $installed ]] || exit 0
latest=$(curl -fsSL --max-time 10 https://api.github.com/repos/verified-human-42/omaplayer/releases/latest |
  grep -o '"tag_name": *"v[^"]*"' | sed 's/.*"v\(.*\)"/\1/')
[[ -n $latest ]] || exit 0
(($(vercmp "$latest-1" "$installed") > 0)) || exit 0
echo "Updating OmaPlayer ${installed%-*} to $latest"
curl -fsSL https://raw.githubusercontent.com/verified-human-42/omaplayer/main/install.sh | bash
HOOK
}

done_message() {
  install_update_hook
  echo
  echo "OmaPlayer is installed. Find it in the app launcher (Super + Space), or run: omaplayer"
  echo "omarchy-update keeps it up to date."
}

# The prebuilt package from the newest release that has one for this machine
# (a release just published is still building its packages for a few
# minutes). pacman pulls in mpv, Qt, and the rest from the Arch repositories.
if [[ $mode == prebuilt ]]; then
  arch=$(uname -m)
  url=$(curl -fsSL "https://api.github.com/repos/$repo/releases?per_page=10" |
    grep -o "\"browser_download_url\": *\"[^\"]*/omaplayer-[0-9][^\"]*-$arch\.pkg\.tar\.zst\"" |
    grep -v -- '-debug-' | head -n1 | sed 's/.*"\(https[^"]*\)"$/\1/') || true
  if [[ -n $url ]]; then
    # Downloaded first: pacman wants a signature for packages it fetches from
    # a URL itself, and the release packages aren't signed.
    work=$(mktemp -d)
    trap 'rm -rf "$work"' EXIT
    curl -fL --progress-bar -o "$work/${url##*/}" "$url"
    make_way_for omaplayer
    sudo pacman -U --needed --noconfirm "$work/${url##*/}"
    done_message
    exit 0
  fi
  echo "No prebuilt package for $arch yet; building it instead."
  mode=build
fi

# Building: makepkg and git, if this machine has never built a package.
package=omaplayer
[[ $mode == git ]] && package=omaplayer-git
sudo pacman -S --needed --noconfirm base-devel git >/dev/null

work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT
curl -fsSL "https://raw.githubusercontent.com/$repo/main/packaging/$package/PKGBUILD" -o "$work/PKGBUILD"
make_way_for "$package"

cd "$work"
# -s installs what's needed to build, -i installs the result; the tests ran
# when the release was made. The compiler's chatter goes to a log, shown
# only when something fails; sudo still asks on the terminal.
log="$work/build.log"
echo "Building OmaPlayer. This takes a minute or two…"
if ! makepkg -si --noconfirm --nocheck >"$log" 2>&1; then
  echo "The build failed. The end of its log:" >&2
  tail -n 30 "$log" >&2
  exit 1
fi
done_message
