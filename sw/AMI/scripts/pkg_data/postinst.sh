#!/bin/bash
set -e

# SPDX-License-Identifier: GPL-2.0-only
# Copyright (c) 2023-2026 Advanced Micro Devices, Inc. All rights reserved.

MOD_NAME="$1"
MOD_VER_STR="$2"
DRIVER_PROFILE="$3"

if [ -z "${MOD_VER_STR}" ]; then
    echo "ERROR: Version not provided to postinst script"
    exit 1
fi

# Default to kernel driver if not specified (for backwards compatibility)
if [ -z "${DRIVER_PROFILE}" ]; then
    DRIVER_PROFILE="kernel"
fi

echo "Installing ${MOD_NAME} ${MOD_VER_STR} (driver profile: ${DRIVER_PROFILE})"

if [ "${MOD_NAME}" = "ami" ]; then
    # Kernel driver installation (only for DRIVER_PROFILE=kernel)
    if [ "${DRIVER_PROFILE}" != "kernel" ]; then
        echo "ERROR: ami package should only be installed with kernel driver profile"
        exit 1
    fi

    # Remove vfio-pci udev rules if they exist (from previous vfio driver installation)
    if [ -f /etc/udev/rules.d/90-ami-vfio.rules ]; then
        echo "Removing vfio-pci udev rules from previous userspace driver installation"
        rm -f /etc/udev/rules.d/90-ami-vfio.rules
        if command -v udevadm >/dev/null 2>&1; then
            udevadm control --reload-rules 2>/dev/null || true
        fi
    fi

    # Remove vfio recovery service if it exists (from previous userspace driver installation)
    if [ -f /etc/systemd/system/ami-vfio-recover.service ]; then
        echo "Removing ami-vfio-recover service from previous userspace driver installation"
        if command -v systemctl >/dev/null 2>&1; then
            systemctl stop ami-vfio-recover.service 2>/dev/null || true
            systemctl disable ami-vfio-recover.service 2>/dev/null || true
        fi
        rm -f /etc/systemd/system/ami-vfio-recover.service
        rm -f /usr/local/sbin/ami-vfio-recover
        if command -v systemctl >/dev/null 2>&1; then
            systemctl daemon-reload 2>/dev/null || true
        fi
    fi

    # Clean up leftover VFIO symlinks from previous userspace driver installation
    echo "Removing VFIO device symlinks (if any)"
    rm -f /dev/ami-bdf-* 2>/dev/null || true

    # Unload vfio-pci modules to prevent conflicts with ami kernel driver
    # This ensures vfio-pci won't auto-bind devices after PCI rescan (during cfgmem operations)
    echo "Unloading vfio-pci modules (if loaded)"
    if lsmod | grep -wq "vfio_pci"; then
        modprobe -r vfio_pci 2>/dev/null || true
        modprobe -r vfio_pci_core 2>/dev/null || true
    fi
    if lsmod | grep -wq "vfio_iommu_type1"; then
        modprobe -r vfio_iommu_type1 2>/dev/null || true
    fi
    if lsmod | grep -wq "vfio"; then
        modprobe -r vfio 2>/dev/null || true
    fi

    # Unbind devices from old ami driver before unloading module
    if lsmod | grep -wq "${MOD_NAME}"; then
        echo "Unbinding devices from old ${MOD_NAME} driver"
        for dev in /sys/bus/pci/devices/*; do
            [ -r "$dev/vendor" ] && [ -r "$dev/class" ] || continue
            [ "$(cat "$dev/vendor")" = "0x10ee" ] || continue
            case "$(cat "$dev/class")" in
                0x1200*) ;;
                *) continue ;;
            esac
            if [ -L "$dev/driver" ]; then
                drv=$(basename "$(readlink -f "$dev/driver")")
                if [ "$drv" = "${MOD_NAME}" ]; then
                    bdf=$(basename "$dev")
                    echo "  Unbinding $bdf from ${MOD_NAME}"
                    echo "$bdf" > "/sys/bus/pci/drivers/${MOD_NAME}/unbind" 2>/dev/null || true
                    sleep 0.5
                fi
            fi
        done

        echo "Unloading old ${MOD_NAME} driver"
        rmmod "${MOD_NAME}" 2>/dev/null || echo "WARNING: could not unload ${MOD_NAME} (in use?); continuing"
    else
        echo "Module ${MOD_NAME} not loaded. Skipping 'rmmod'."
    fi

    # Remove all old Dynamic Kernel Module Support(DKMS) installed versions
    # Force remove any existing ami modules in DKMS tree
    echo "Cleaning up old ${MOD_NAME} DKMS modules"
    while dkms status -m "${MOD_NAME}" 2>/dev/null | grep -q "${MOD_NAME}"; do
        # Extract version from DKMS status (handles both DKMS v2 and v3+ formats)
        MOD_VER_STR_OLD=$(dkms status -m "${MOD_NAME}" 2>/dev/null | head -n1 | sed -n 's/.*ami[/,][[:space:]]*\([^,/:[:space:]]*\).*/\1/p')
        if [ -n "${MOD_VER_STR_OLD}" ]; then
            echo "Removing ${MOD_NAME} version ${MOD_VER_STR_OLD} from DKMS tree"
            dkms remove -m "${MOD_NAME}" -v "${MOD_VER_STR_OLD}" --all 2>/dev/null || true
            # If that version is still present, the removal didn't take effect;
            # break instead of re-parsing the same version and looping forever.
            if dkms status -m "${MOD_NAME}" -v "${MOD_VER_STR_OLD}" 2>/dev/null | grep -q "${MOD_NAME}"; then
                echo "WARNING: ${MOD_NAME} ${MOD_VER_STR_OLD} still present after remove; aborting DKMS cleanup"
                break
            fi
        else
            # Fallback: force remove without version if parsing fails
            echo "Force removing ${MOD_NAME} from DKMS tree"
            dkms remove "${MOD_NAME}/${MOD_VER_STR}" --all 2>/dev/null || true
            break
        fi
    done

    # Remove old kernel module files
    echo "Removing old ${MOD_NAME} kernel module files"
    find /lib/modules -type f -name "${MOD_NAME}.ko*" -delete 2>/dev/null || true

    depmod -A

    # Add and install new DKMS module
    echo "Adding ${MOD_NAME} ${MOD_VER_STR} to DKMS"
    # Check if already added (from previous failed install attempt)
    if dkms status -m "${MOD_NAME}" -v "${MOD_VER_STR}" 2>/dev/null | grep -q "added"; then
        echo "${MOD_NAME} ${MOD_VER_STR} already added to DKMS, skipping add step"
    elif ! dkms add -m "${MOD_NAME}" -v "${MOD_VER_STR}" 2>&1; then
        # If add fails, check if it's because module already exists
        if dkms status -m "${MOD_NAME}" -v "${MOD_VER_STR}" 2>/dev/null | grep -q "${MOD_NAME}"; then
            echo "${MOD_NAME} ${MOD_VER_STR} already in DKMS tree, continuing"
        else
            echo "ERROR: dkms add failed"
            exit 1
        fi
    fi

    echo "Installing ${MOD_NAME} ${MOD_VER_STR} with DKMS"
    if ! dkms install -m "${MOD_NAME}" -v "${MOD_VER_STR}"; then
        echo "ERROR: dkms install failed"
        exit 1
    fi

    echo "INFO: Loading new ${MOD_NAME} Linux kernel module"
    modprobe "${MOD_NAME}"

    if ! dkms status -m "${MOD_NAME}" -v "${MOD_VER_STR}" | grep -q installed; then
        echo "ERROR: failed to install ${MOD_NAME} drivers"
        exit 1
    fi

    # Bind ami kernel driver to AMI devices (handles ami reinstall, vfio→ami switch, and fresh devices)
    echo "Binding AMI devices to ami kernel driver..."
    for dev in /sys/bus/pci/devices/*; do
        [ -r "$dev/vendor" ] && [ -r "$dev/device" ] || continue
        vendor_id=$(cat "$dev/vendor")
        [ "$vendor_id" = "0x10ee" ] || continue

        device_id=$(cat "$dev/device")
        bdf=$(basename "$dev")

        # Match known AMI PF0 device IDs (only PF0 binds to driver)
        case "$device_id" in
            0x50b4|0x50f4|0x5710) ;;  # V80, RAVE, etc. - PF0 only
            *) continue ;;
        esac

        # Check current driver binding
        current_drv=""
        if [ -L "$dev/driver" ]; then
            current_drv=$(basename "$(readlink -f "$dev/driver")")
        fi

        if [ "$current_drv" = "ami" ]; then
            echo "  $bdf: already bound to ami"
            continue
        fi

        # Unbind from current driver if needed
        if [ -n "$current_drv" ]; then
            if [ "$current_drv" = "vfio-pci" ]; then
                echo "  $bdf: unbinding from vfio-pci"
                echo "$bdf" > /sys/bus/pci/drivers/vfio-pci/unbind 2>/dev/null || true
            else
                echo "  $bdf: unbinding from $current_drv"
                echo "$bdf" > "/sys/bus/pci/drivers/$current_drv/unbind" 2>/dev/null || true
            fi
            sleep 0.5
        fi

        # Clear driver_override (vfio-pci sets this)
        echo "" > "$dev/driver_override" 2>/dev/null || true

        # Bind to ami driver using new_id
        vendor_clean=$(echo "$vendor_id" | sed 's/0x//')
        device_clean=$(echo "$device_id" | sed 's/0x//')
        echo "  $bdf: binding to ami driver"
        echo "$vendor_clean $device_clean" > /sys/bus/pci/drivers/ami/new_id 2>/dev/null || true
        sleep 0.2

        # Verify binding succeeded
        if [ -L "$dev/driver" ]; then
            new_drv=$(basename "$(readlink -f "$dev/driver")")
            if [ "$new_drv" = "ami" ]; then
                echo "  $bdf: successfully bound to ami"
            else
                echo "  WARNING: $bdf bound to $new_drv instead of ami" >&2
            fi
        else
            echo "  WARNING: $bdf failed to bind to ami" >&2
        fi
    done

    echo "SUCCESS: ${MOD_NAME} driver ${MOD_VER_STR} installed and loaded"

elif [ "${MOD_NAME}" = "libami" ]; then
    echo "Removing old ${MOD_NAME} library files"
    find /usr/local/lib -type f -name "libami.so*" -delete 2>/dev/null || true
    find /usr/local/lib -type f -name "libgcq_userspace.so*" -delete 2>/dev/null || true

    echo "Building ${MOD_NAME} ${MOD_VER_STR} with ${DRIVER_PROFILE} driver"
    cd "/usr/src/${MOD_NAME}-${MOD_VER_STR}/api" || exit 1
    make clean
    make DRIVER_PROFILE=${DRIVER_PROFILE}

    echo "Installing libami.so"
    VERSION_MAJOR=$(echo "${MOD_VER_STR}" | cut -d. -f1)
    VERSION_MINOR=$(echo "${MOD_VER_STR}" | cut -d. -f2)
    mkdir -p /usr/local/lib/
    install -m 755 build/libami.so "/usr/local/lib/libami.so.${VERSION_MAJOR}.${VERSION_MINOR}.0"
    ln -sf "/usr/local/lib/libami.so.${VERSION_MAJOR}.${VERSION_MINOR}.0" /usr/local/lib/libami.so

    echo "Installing headers"
    mkdir -p /usr/local/include/ami
    cp -f "/usr/src/${MOD_NAME}-${MOD_VER_STR}/api/include"/*.h /usr/local/include/ami/

    ldconfig

    echo "SUCCESS: ${MOD_NAME} ${MOD_VER_STR} installed"
    if [ "${DRIVER_PROFILE}" = "userspace" ]; then
        echo "Note: VFIO userspace driver. Requires IOMMU and vfio-pci."
    else
        echo "Note: Kernel driver. Requires ami module loaded."
    fi

else
    # amitool
    echo "Removing old ami_tool binary"
    find /usr/local/bin -type f -name "ami_tool" -delete 2>/dev/null || true

    echo "Building API library for ${MOD_NAME} ${MOD_VER_STR}"
    cd "/usr/src/${MOD_NAME}-${MOD_VER_STR}/api" || exit 1
    make clean
    make DRIVER_PROFILE=${DRIVER_PROFILE}

    echo "Building ami_tool"
    cd "/usr/src/${MOD_NAME}-${MOD_VER_STR}/app" || exit 1
    make clean
    make
    install -m 755 build/ami_tool /usr/local/bin/

    if [ "${DRIVER_PROFILE}" = "userspace" ]; then
        # VFIO-specific setup: recovery tools, udev rules, systemd service
        VFIO_SRC_DIR="/usr/src/${MOD_NAME}-${MOD_VER_STR}/scripts"

        if [ -f "${VFIO_SRC_DIR}/recover_vfio_device.sh" ]; then
            echo "Installing VFIO recovery tool: /usr/local/sbin/ami-vfio-recover"
            install -m 755 "${VFIO_SRC_DIR}/recover_vfio_device.sh" /usr/local/sbin/ami-vfio-recover
        fi

        echo "Installing udev rules: /etc/udev/rules.d/90-ami-vfio.rules"
        mkdir -p /etc/udev/rules.d
        cat > /etc/udev/rules.d/90-ami-vfio.rules <<'EOF'
# Set driver override and bind to vfio-pci for AMI devices (PF0 only - 0x50b4, 0x50f4, 0x5710)
ACTION=="add", SUBSYSTEM=="pci", ATTR{vendor}=="0x10ee", ATTR{device}=="0x50b4", \
  ATTR{driver_override}="vfio-pci", \
  RUN+="/bin/sh -c 'echo %k > /sys/bus/pci/drivers/vfio-pci/bind 2>/dev/null || true'"

ACTION=="add", SUBSYSTEM=="pci", ATTR{vendor}=="0x10ee", ATTR{device}=="0x50f4", \
  ATTR{driver_override}="vfio-pci", \
  RUN+="/bin/sh -c 'echo %k > /sys/bus/pci/drivers/vfio-pci/bind 2>/dev/null || true'"

ACTION=="add", SUBSYSTEM=="pci", ATTR{vendor}=="0x10ee", ATTR{device}=="0x5710", \
  ATTR{driver_override}="vfio-pci", \
  RUN+="/bin/sh -c 'echo %k > /sys/bus/pci/drivers/vfio-pci/bind 2>/dev/null || true'"

# For AMI VFIO groups: create BDF symlink and relax permissions
SUBSYSTEM=="vfio", ACTION=="add", KERNEL=="[0-9]*", \
  RUN+="/bin/sh -c 'g=\"$${DEVNAME##*/}\"; \
    for p in /sys/bus/pci/devices/*; do \
      [ \"$$(basename $$(readlink $$p/iommu_group 2>/dev/null))\" = \"$$g\" ] || continue; \
      v=$$(cat $$p/vendor 2>/dev/null); d=$$(cat $$p/device 2>/dev/null); \
      { [ \"$$v\" = \"0x10ee\" ] && { [ \"$$d\" = \"0x50b4\" ] || [ \"$$d\" = \"0x50f4\" ] || [ \"$$d\" = \"0x5710\" ]; }; } || continue; \
      ln -sf \"$$DEVNAME\" /dev/ami-bdf-$$(basename $$p | cut -d: -f2-); \
      chmod 0666 \"$$DEVNAME\"; \
    done'"

# Remove symlinks when VFIO group is removed
SUBSYSTEM=="vfio", ACTION=="remove", KERNEL=="[0-9]*", \
  RUN+="/bin/sh -c 'rm -f /dev/ami-bdf-*'"
EOF
        chmod 644 /etc/udev/rules.d/90-ami-vfio.rules

        # Detect Alveo devices and create systemd service
        HAVE_AMR=0
        for dev in /sys/bus/pci/devices/*; do
            [ -r "$dev/vendor" ] && [ -r "$dev/class" ] || continue
            [ "$(cat "$dev/vendor")" = "0x10ee" ] || continue
            case "$(cat "$dev/class")" in
                0x1200*) HAVE_AMR=1; break ;;
            esac
        done

        if [ "${HAVE_AMR}" -eq 1 ] && [ -x /usr/local/sbin/ami-vfio-recover ]; then
            echo "Creating ami-vfio-recover.service"
            cat > /etc/systemd/system/ami-vfio-recover.service <<'EOF'
[Unit]
Description=Recover AMI VFIO binding for Alveo devices
After=systemd-udev-settle.service
Wants=systemd-udev-settle.service

[Service]
Type=oneshot
ExecStart=/usr/local/sbin/ami-vfio-recover --all --skip-verify
TimeoutStartSec=120

[Install]
WantedBy=multi-user.target
EOF
            chmod 644 /etc/systemd/system/ami-vfio-recover.service
        fi

        # Load vfio modules for userspace driver
        # This ensures vfio-pci can bind to devices (for userspace driver operation)
        # Best-effort activation; never fail the package install on a host with no
        # device, no IOMMU, or no systemd (e.g. containers, build farms).
        echo "Loading vfio-pci modules for userspace driver"
        modprobe vfio 2>/dev/null || true
        modprobe vfio_iommu_type1 2>/dev/null || true
        modprobe vfio-pci 2>/dev/null || true
        if command -v udevadm >/dev/null 2>&1; then
            udevadm control --reload-rules 2>/dev/null || true
            udevadm trigger --subsystem-match=pci 2>/dev/null || true
            udevadm trigger --subsystem-match=vfio 2>/dev/null || true
        fi

        # Unbind and unload ami kernel module if auto-loaded by udev (prevents conflict)
        # udevadm trigger above can auto-load ami module from DKMS, so check again here
        if lsmod | grep -wq "ami"; then
            echo "Unbinding devices from ami kernel module (auto-loaded by udev)"
            for dev in /sys/bus/pci/devices/*; do
                [ -r "$dev/vendor" ] && [ -r "$dev/class" ] || continue
                [ "$(cat "$dev/vendor")" = "0x10ee" ] || continue
                case "$(cat "$dev/class")" in
                    0x1200*) ;;
                    *) continue ;;
                esac
                if [ -L "$dev/driver" ]; then
                    drv=$(basename "$(readlink -f "$dev/driver")")
                    if [ "$drv" = "ami" ]; then
                        bdf=$(basename "$dev")
                        echo "  Unbinding $bdf from ami"
                        echo "$bdf" > /sys/bus/pci/drivers/ami/unbind 2>/dev/null || true
                    fi
                fi
            done
            sleep 0.5
            echo "Unloading ami kernel module"
            rmmod ami 2>/dev/null || true
        fi

        # Explicitly bind Alveo devices to vfio-pci if not already bound
        echo "Binding AMI devices to vfio-pci driver..."
        for dev in /sys/bus/pci/devices/*; do
            [ -r "$dev/vendor" ] && [ -r "$dev/device" ] || continue
            [ "$(cat "$dev/vendor")" = "0x10ee" ] || continue
            device_id=$(cat "$dev/device")
            case "$device_id" in
                0x50b4|0x50f4|0x5710) ;;
                *) continue ;;
            esac
            bdf=$(basename "$dev")
            # Skip if already bound to vfio-pci
            if [ -L "$dev/driver" ]; then
                drv=$(basename "$(readlink -f "$dev/driver")")
                if [ "$drv" = "vfio-pci" ]; then
                    echo "  $bdf already bound to vfio-pci"
                    continue
                fi
            fi
            # Bind to vfio-pci
            echo "  Binding $bdf to vfio-pci"
            echo "vfio-pci" > "$dev/driver_override" 2>/dev/null || true
            echo "$bdf" > /sys/bus/pci/drivers/vfio-pci/bind 2>/dev/null || true
        done

        if command -v systemctl >/dev/null 2>&1 && [ -f /etc/systemd/system/ami-vfio-recover.service ]; then
            systemctl daemon-reload 2>/dev/null || true
            systemctl enable --now ami-vfio-recover.service 2>/dev/null || true
        fi
    fi

    echo "SUCCESS: ${MOD_NAME} ${MOD_VER_STR} installed"
fi

echo "Done!"
