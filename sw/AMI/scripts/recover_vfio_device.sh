/**
 * Copyright (C) 2026 Advanced Micro Devices, Inc. All rights reserved.
 * SPDX-License-Identifier: MIT
 *
 * recover_vfio_device.sh - This script recovers VFIO usability after hot-resets or transient states
**/
#!/usr/bin/env bash
set -euo pipefail

# Recover AMI VFIO usability after hot-reset/transient states.
# Rebinds all devices in the target IOMMU group(s) to vfio-pci and verifies AMI readiness.
# Supports one or more devices via repeated -d, or --all to auto-discover every Alveo device.
#
# Usage examples:
#   sudo ./recover_vfio_device.sh --all
#   sudo ./recover_vfio_device.sh -d 0000:e2:00.0
#   sudo ./recover_vfio_device.sh -d 0000:e2:00.0 -d 0000:c1:00.0
#   sudo ./recover_vfio_device.sh -d e2:00.0 -g 76 --skip-verify

DEFAULT_BDF="0000:e2:00.0"
ALVEO_VENDOR="0x10ee"
BDFS=()
GROUP=""
ALL=0
VERIFY=0
OVERVIEW_TIMEOUT_SEC=25
DEBUG_TIMEOUT_SEC=25

usage() {
  cat <<'EOF'
Recover AMI VFIO state by re-binding all devices in an IOMMU group to vfio-pci.

Options:
  -d, --device <BDF>   PCI BDF (repeatable; accepts e2:00.0 too)
  --all                Recover every Alveo device found on the system
  -g, --group  <N>     IOMMU group number (only valid with a single -d)
  --verify             Run ami_tool validation checks after rebind (default: off)
  --overview-timeout N Timeout in seconds for ami_tool overview (default: 25)
  --debug-timeout N    Timeout in seconds for debug_verbosity check (default: 25)
  --skip-verify        Backward-compatible alias (same as default behavior)
  -h, --help           Show this help

If neither -d nor --all is given, defaults to a single device (0000:e2:00.0).
EOF
}

normalize_bdf() {
  local in="$1"
  if [[ "$in" =~ ^[0-9a-fA-F]{4}:[0-9a-fA-F]{2}:[0-9a-fA-F]{2}\.[0-7]$ ]]; then
    printf '%s\n' "${in,,}"
  elif [[ "$in" =~ ^[0-9a-fA-F]{2}:[0-9a-fA-F]{2}\.[0-7]$ ]]; then
    printf '0000:%s\n' "${in,,}"
  else
    echo "Invalid BDF format: $in" >&2
    exit 2
  fi
}

log() { echo "[INFO] $*"; }
warn() { echo "[WARN] $*"; }
err() { echo "[ERROR] $*" >&2; }

# Return 0 if the sysfs device path is a Xilinx processing-accelerator (Alveo).
is_alveo() {
  local d="$1"
  [[ -r "$d/vendor" && -r "$d/class" ]] || return 1
  [[ "$(cat "$d/vendor")" == "$ALVEO_VENDOR" ]] || return 1
  case "$(cat "$d/class")" in
    0x1200*) return 0 ;;
    *) return 1 ;;
  esac
}

while [[ $# -gt 0 ]]; do
  case "$1" in
    -d|--device)
      [[ $# -ge 2 ]] || { err "Missing value for $1"; exit 2; }
      BDFS+=("$(normalize_bdf "$2")")
      shift 2
      ;;
    --all)
      ALL=1
      shift
      ;;
    -g|--group)
      [[ $# -ge 2 ]] || { err "Missing value for $1"; exit 2; }
      GROUP="$2"
      shift 2
      ;;
    --verify)
      VERIFY=1
      shift
      ;;
    --overview-timeout)
      [[ $# -ge 2 ]] || { err "Missing value for $1"; exit 2; }
      OVERVIEW_TIMEOUT_SEC="$2"
      shift 2
      ;;
    --debug-timeout)
      [[ $# -ge 2 ]] || { err "Missing value for $1"; exit 2; }
      DEBUG_TIMEOUT_SEC="$2"
      shift 2
      ;;
    --skip-verify)
      VERIFY=0
      shift
      ;;
    -h|--help)
      usage
      exit 0
      ;;
    *)
      err "Unknown argument: $1"
      usage
      exit 2
      ;;
  esac
done

if [[ "$EUID" -ne 0 ]]; then
  err "Run as root (use sudo)."
  exit 1
fi

if ! [[ "$OVERVIEW_TIMEOUT_SEC" =~ ^[0-9]+$ ]] || ! [[ "$DEBUG_TIMEOUT_SEC" =~ ^[0-9]+$ ]]; then
  err "Timeout values must be non-negative integers"
  exit 2
fi

# Build the list of target BDFs.
if [[ $ALL -eq 1 ]]; then
  for dev in /sys/bus/pci/devices/*; do
    is_alveo "$dev" || continue
    BDFS+=("$(basename "$dev")")
  done
  if [[ ${#BDFS[@]} -eq 0 ]]; then
    log "No Alveo devices found; nothing to recover."
    exit 0
  fi
elif [[ ${#BDFS[@]} -eq 0 ]]; then
  BDFS=("$DEFAULT_BDF")
fi

# -g only makes sense for a single explicit device.
if [[ -n "$GROUP" && ( $ALL -eq 1 || ${#BDFS[@]} -gt 1 ) ]]; then
  warn "-g/--group is ignored when multiple devices or --all are specified"
  GROUP=""
fi

if [[ -n "$GROUP" ]] && ! [[ "$GROUP" =~ ^[0-9]+$ ]]; then
  err "Invalid IOMMU group: $GROUP"
  exit 2
fi

# Ensure VFIO modules are available.
modprobe vfio || true
modprobe vfio-pci || true
modprobe vfio_iommu_type1 || true

# Resolve unique IOMMU groups across all target BDFs.
# NOTE: do not name this array GROUPS — that is a bash special variable holding
# the current user's group IDs, and assignments to it are silently ignored.
declare -A GROUP_SEEN
IOMMU_GROUPS=()
for bdf in "${BDFS[@]}"; do
  dpath="/sys/bus/pci/devices/${bdf}"
  if [[ ! -d "$dpath" ]]; then
    warn "Device not found in sysfs: ${bdf} (skipping)"
    continue
  fi

  if [[ -n "$GROUP" ]]; then
    g="$GROUP"
  elif [[ -L "$dpath/iommu_group" ]]; then
    g="$(basename "$(readlink -f "$dpath/iommu_group")")"
  else
    warn "Could not detect IOMMU group for ${bdf} (skipping)"
    continue
  fi

  if [[ -z "${GROUP_SEEN[$g]:-}" ]]; then
    GROUP_SEEN[$g]=1
    IOMMU_GROUPS+=("$g")
    log "Target ${bdf} -> IOMMU group ${g}"
  fi
done

if [[ ${#IOMMU_GROUPS[@]} -eq 0 ]]; then
  err "No valid IOMMU groups to recover"
  exit 1
fi

# Rebind every function in a single IOMMU group to vfio-pci. Returns non-zero on failure.
recover_group() {
  local group="$1"
  local group_path="/sys/kernel/iommu_groups/${group}/devices"
  local dev drv_link drv_name vendor device ven_hex dev_hex drv

  if [[ ! -d "$group_path" ]]; then
    err "IOMMU group path missing: $group_path"
    return 1
  fi

  local group_devs=()
  mapfile -t group_devs < <(ls -1 "$group_path" | sort)
  if [[ ${#group_devs[@]} -eq 0 ]]; then
    err "No devices found in IOMMU group ${group}"
    return 1
  fi

  log "Group ${group} devices: ${group_devs[*]}"

  # Unbind any currently bound driver for each function in the group.
  for dev in "${group_devs[@]}"; do
    drv_link="/sys/bus/pci/devices/${dev}/driver"
    if [[ -L "$drv_link" ]]; then
      drv_name="$(basename "$(readlink -f "$drv_link")")"
      log "Unbinding ${dev} from ${drv_name}"
      echo "$dev" > "/sys/bus/pci/drivers/${drv_name}/unbind"
    else
      log "${dev} already unbound"
    fi
  done

  # Bind only the AMI management function (PCIe function 0) to vfio-pci.
  # Secondary functions (e.g. 0x50b5) are intentionally left unbound: the AMI
  # stack only operates on function 0, and the IOMMU group stays VFIO-viable as
  # long as no member is bound to a foreign (non-vfio-pci) driver.
  local func bound_any=0
  for dev in "${group_devs[@]}"; do
    func="${dev##*.}"
    if ! is_alveo "/sys/bus/pci/devices/${dev}" || [[ "$func" != "0" ]]; then
      log "Skipping ${dev} (not an AMI management function)"
      continue
    fi

    vendor="$(<"/sys/bus/pci/devices/${dev}/vendor")"   # e.g. 0x10ee
    device="$(<"/sys/bus/pci/devices/${dev}/device")"   # e.g. 0x50b4
    ven_hex="${vendor#0x}"
    dev_hex="${device#0x}"

    # new_id may fail with EEXIST if already present; ignore failures.
    echo "${ven_hex} ${dev_hex}" > /sys/bus/pci/drivers/vfio-pci/new_id 2>/dev/null || true

    log "Binding ${dev} to vfio-pci"
    if ! echo "$dev" > /sys/bus/pci/drivers/vfio-pci/bind 2>/tmp/ami_recover_bind.err; then
      if [[ -L "/sys/bus/pci/devices/${dev}/driver" ]] && \
         [[ "$(basename "$(readlink -f "/sys/bus/pci/devices/${dev}/driver")")" == "vfio-pci" ]]; then
        warn "Bind returned error for ${dev}, but driver is already vfio-pci"
      else
        err "Failed to bind ${dev} to vfio-pci: $(tr -d '\n' </tmp/ami_recover_bind.err)"
        return 1
      fi
    fi
    bound_any=1
  done

  if [[ "$bound_any" -eq 0 ]]; then
    err "No AMI management function (PCIe function 0) found in IOMMU group ${group}"
    return 1
  fi

  # Verify: the management function(s) must be on vfio-pci; every other group
  # member must simply not be bound to a foreign driver (unbound is fine).
  for dev in "${group_devs[@]}"; do
    func="${dev##*.}"
    if [[ -L "/sys/bus/pci/devices/${dev}/driver" ]]; then
      drv="$(basename "$(readlink -f "/sys/bus/pci/devices/${dev}/driver")")"
    else
      drv=""
    fi

    if is_alveo "/sys/bus/pci/devices/${dev}" && [[ "$func" == "0" ]]; then
      if [[ "$drv" != "vfio-pci" ]]; then
        err "${dev} is bound to ${drv:-none}, expected vfio-pci"
        return 1
      fi
    elif [[ -n "$drv" && "$drv" != "vfio-pci" ]]; then
      err "${dev} bound to foreign driver ${drv}; IOMMU group ${group} not VFIO-viable"
      return 1
    fi
  done

  if [[ ! -e "/dev/vfio/${group}" ]]; then
    warn "/dev/vfio/${group} missing; udev/system state may still be settling"
  fi

  log "Group ${group} is now bound to vfio-pci"
  return 0
}

overall_rc=0
for g in "${IOMMU_GROUPS[@]}"; do
  log "Recovering IOMMU group ${g}"
  if ! recover_group "$g"; then
    err "Recovery failed for IOMMU group ${g}"
    overall_rc=1
  fi
done

if [[ $overall_rc -ne 0 ]]; then
  err "One or more IOMMU groups failed to recover"
  exit 1
fi

log "All targeted IOMMU groups are now bound to vfio-pci"

if [[ "$VERIFY" -ne 1 ]]; then
  log "Recovery rebind completed (verification disabled by default)."
  log "To run checks explicitly, use: --verify"
  exit 0
fi

if ! command -v ami_tool >/dev/null 2>&1; then
  warn "ami_tool not found in PATH; skipping command verification"
  exit 0
fi

# Verify readiness with overview and a lightweight per-device command.
log "Running: ami_tool overview"
if ! timeout -k 2 "${OVERVIEW_TIMEOUT_SEC}" ami_tool overview >/tmp/ami_recover_overview.out 2>/tmp/ami_recover_overview.err; then
  warn "overview timed out or failed after ${OVERVIEW_TIMEOUT_SEC}s; see /tmp/ami_recover_overview.err"
fi

for bdf in "${BDFS[@]}"; do
  short_bdf="${bdf#0000:}"
  if grep -qF "${short_bdf}" /tmp/ami_recover_overview.out 2>/dev/null; then
    log "overview lists ${short_bdf}"
  else
    warn "overview did not list ${short_bdf}; check /tmp/ami_recover_overview.out"
  fi

  log "Running: ami_tool debug_verbosity -d ${short_bdf} -l debug"
  if timeout -k 2 "${DEBUG_TIMEOUT_SEC}" ami_tool debug_verbosity -d "${short_bdf}" -l debug >/tmp/ami_recover_debug.out 2>/tmp/ami_recover_debug.err; then
    log "debug_verbosity succeeded for ${short_bdf}"
  else
    warn "debug_verbosity timed out or failed for ${short_bdf}; see /tmp/ami_recover_debug.err"
  fi
done

log "Recovery script completed"
