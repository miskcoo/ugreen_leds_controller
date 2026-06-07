#!/bin/bash
#
# extract-truenas-headers.sh
#
# Build linux-headers-truenas-production-amd64.deb directly from a TrueNAS-SCALE
# update file (TrueNAS-SCALE-<ver>.update). No ISO handling required.
#
# Style: Google Shell Style Guide (2-space indent, main(), lower_case funcs).

set -euo pipefail

#######################################
# Constants
#######################################
readonly PKG="linux-headers-truenas-production-amd64"
# iX SecTeam <security-officer@ixsystems.com> - signs TrueNAS releases/updates.
# Override only for testing:  IX_FPR=<fpr> ./extract-truenas-headers.sh ...
readonly IX_FPR="${IX_FPR:-C8D62DEF767C1DB0DFF4E6EC358EAA9112CF7946}"

# Scratch dir (global so the EXIT trap can reach it after main() returns).
work_dir=""

#######################################
# Remove the scratch directory. Registered as the EXIT trap.
#######################################
cleanup() {
  [[ -n "${work_dir}" ]] && rm -rf "${work_dir}"
}

#######################################
# Print a message to stderr.
#######################################
err() {
  echo "$@" >&2
}

#######################################
# Print a message to stderr and exit non-zero.
#######################################
die() {
  err "Error: $*"
  exit 1
}

#######################################
# Print usage and exit 0.
#######################################
usage() {
  cat <<'USAGE'
Usage:
  extract-truenas-headers.sh [options] <TrueNAS-SCALE-x.y.z.update> [output-dir]

Options:
  --sig <file>   Signature file to use (default: <update>.sig if present)
  --key <file>   Import the iX public key from a file instead of a keyserver
  --no-verify    Skip signature verification (not recommended)
  -h, --help     Show this help

Signature verification:
  Verifies the .update against the iX SecTeam key and ensures the primary
  fingerprint exactly matches the pinned trust anchor. If the key is not in
  the keyring it is fetched from a keyserver; when offline, supply it with
  --key <pubkey.asc>. Show the fingerprint with:
    gpg --fingerprint security-officer@ixsystems.com

Notes:
  unsquashfs needs ~2 GB of scratch space. If /tmp is too small, set TMPDIR:
    TMPDIR=/mnt/pool/tmp extract-truenas-headers.sh ...
USAGE
  exit 0
}

#######################################
# Verify that required commands exist.
# Arguments:
#   List of command names.
#######################################
require_tools() {
  local tool
  for tool in "$@"; do
    command -v "${tool}" >/dev/null || die "'${tool}' not in PATH"
  done
}

#######################################
# Verify a detached signature against the pinned iX key.
# Globals:
#   IX_FPR (read)
# Arguments:
#   1: file that was signed
#   2: detached signature file
#   3: optional public-key file to import (may be empty)
#######################################
verify_signature() {
  local file="$1"
  local sig="$2"
  local key_file="$3"

  require_tools gpg

  # Make sure the pinned key is available in the keyring.
  if ! gpg --list-keys "${IX_FPR}" >/dev/null 2>&1; then
    if [[ -n "${key_file}" ]]; then
      gpg --import "${key_file}" >/dev/null 2>&1 \
        || die "importing '${key_file}' failed"
    else
      err "    iX key not in keyring, trying keyserver..."
      gpg --batch --keyserver hkps://keys.openpgp.org \
        --keyserver-options timeout=15 \
        --recv-keys "${IX_FPR}" >/dev/null 2>&1 || true
    fi
  fi
  if ! gpg --list-keys "${IX_FPR}" >/dev/null 2>&1; then
    err "Error: iX public key (${IX_FPR}) not available."
    err "       Import with:  gpg --recv-keys ${IX_FPR}"
    err "       or a file:    --key <pubkey.asc>   (or --no-verify)"
    exit 1
  fi

  # Verify, then confirm the *primary* fingerprint matches the pin so that a
  # valid signature from any other key cannot pass.
  local status primary
  status="$(LC_ALL=C gpg --status-fd 1 --verify "${sig}" "${file}" 2>/dev/null)" \
    || die "signature verification FAILED for '${file}'"

  primary="$(awk '/^\[GNUPG:\] VALIDSIG/ {print $NF}' <<<"${status}")"
  if [[ "${primary}" != "${IX_FPR}" ]]; then
    err "Error: signature is NOT from the expected iX key"
    err "       expected: ${IX_FPR}"
    err "       got:      ${primary:-<no valid signature>}"
    exit 1
  fi

  echo "    Signature OK -> iX SecTeam (${IX_FPR})"
}

#######################################
# Main
#######################################
main() {
  local sig_file="" key_file="" no_verify=0
  local -a posargs=()

  while [[ $# -gt 0 ]]; do
    case "$1" in
      --sig)       sig_file="${2:?--sig requires a path}"; shift 2 ;;
      --key)       key_file="${2:?--key requires a path}"; shift 2 ;;
      --no-verify) no_verify=1; shift ;;
      -h|--help)   usage ;;
      --)          shift; while [[ $# -gt 0 ]]; do posargs+=("$1"); shift; done ;;
      -*)          die "unknown option '$1'" ;;
      *)           posargs+=("$1"); shift ;;
    esac
  done

  local update_file="${posargs[0]:?Usage: $0 [options] <*.update file> [output-dir]}"
  local output_dir="${posargs[1]:-.}"

  [[ -f "${update_file}" ]] || die "'${update_file}' not found"

  require_tools unsquashfs dpkg-deb awk

  work_dir="$(mktemp -d "${TMPDIR:-/tmp}/truenas-headers.XXXXXX")"
  trap cleanup EXIT

  # ---- 0) signature verification ----------------------------------------
  if (( no_verify )); then
    echo "[0/4] Signature verification skipped (--no-verify)"
  else
    if [[ -z "${sig_file}" && -f "${update_file}.sig" ]]; then
      sig_file="${update_file}.sig"
    fi
    [[ -n "${sig_file}" ]] \
      || die "no signature found (${update_file}.sig missing); use --sig or --no-verify"
    [[ -f "${sig_file}" ]] || die "signature '${sig_file}' not found"

    echo "[0/4] Verifying signature (${sig_file})..."
    verify_signature "${update_file}" "${sig_file}" "${key_file}"
  fi

  # ---- 1) pull rootfs.squashfs out of the .update -----------------------
  echo "[1/4] Extracting rootfs.squashfs from the update file..."
  unsquashfs -d "${work_dir}/outer" "${update_file}" rootfs.squashfs >/dev/null
  local rootfs="${work_dir}/outer/rootfs.squashfs"
  [[ -f "${rootfs}" ]] || die "rootfs.squashfs not found in the update file"

  # ---- 2) package metadata ----------------------------------------------
  echo "[2/4] Reading package metadata..."
  unsquashfs -d "${work_dir}/dpkg" "${rootfs}" \
    "var/lib/dpkg/status" \
    "var/lib/dpkg/info/${PKG}.md5sums" >/dev/null

  local status_file="${work_dir}/dpkg/var/lib/dpkg/status"
  grep -q "^Package: ${PKG}\$" "${status_file}" \
    || die "${PKG} not installed in rootfs"

  local pkg_version
  pkg_version="$(awk -v pkg="${PKG}" '
    $0 == "Package: " pkg {f = 1}
    f && /^$/                {exit}
    f && /^Version:/         {print $2; exit}
  ' "${status_file}")"
  [[ -n "${pkg_version}" ]] || die "could not read Version"

  # Kernel version = package version without the Debian revision suffix.
  local kernel_version="${pkg_version%-*}"
  echo "    Version: ${pkg_version}  (kernel ${kernel_version})"

  # ---- 3) header tree + modules/build symlink ---------------------------
  echo "[3/4] Extracting header files..."
  unsquashfs -d "${work_dir}/files" "${rootfs}" \
    "usr/src/${PKG}" \
    "usr/share/doc/${PKG}" \
    "usr/lib/modules/${kernel_version}/build" >/dev/null
  [[ -d "${work_dir}/files/usr/src/${PKG}" ]] || die "header files missing"

  rm -f "${rootfs}"  # free ~1.8 GB before packing

  # ---- 4) build the .deb ------------------------------------------------
  echo "[4/4] Building .deb..."
  local deb_root="${work_dir}/deb"
  mkdir -p "${deb_root}/DEBIAN"

  # control = the full dpkg stanza minus dpkg-internal fields.
  awk -v pkg="${PKG}" '
    $0 == "Package: " pkg     {f = 1}
    f && /^$/                  {exit}
    f && /^Status:/            {next}
    f && /^Config-Version:/    {next}
    f                          {print}
  ' "${status_file}" >"${deb_root}/DEBIAN/control"
  [[ -s "${deb_root}/DEBIAN/control" ]] || die "empty control file"

  cp "${work_dir}/dpkg/var/lib/dpkg/info/${PKG}.md5sums" \
    "${deb_root}/DEBIAN/md5sums"
  cp -a "${work_dir}/files/usr" "${deb_root}/"

  mkdir -p "${output_dir}"
  local deb_file="${output_dir}/${PKG}_${pkg_version}_amd64.deb"
  dpkg-deb --build --root-owner-group "${deb_root}" "${deb_file}"

  echo "Done -> ${deb_file} ($(du -sh "${deb_file}" | cut -f1))"
}

main "$@"
