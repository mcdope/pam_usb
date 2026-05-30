#!/usr/bin/bash
# Boots a QEMU full-system VM, installs a pam_usb .deb, and runs the
# functional test suite inside the VM.
#
# Host prerequisites:
#   arm64: qemu-system-aarch64, qemu-efi-aarch64, cloud-image-utils
#   armhf: qemu-system-arm, qemu-efi-arm, cloud-image-utils
#
# Usage: run-tests-in-qemu.sh <arch> <deb_path>
#   arch     : arm64 | armhf
#   deb_path : path to the libpam-usb .deb package to test
#
# Golden image strategy:
#   First run: provisions a golden qcow2 image with all packages, kernel
#   modules (including g_mass_storage via linux-modules-extra), and
#   dummy_hcd pre-built. A fixed SSH keypair is embedded; cloud-init is
#   disabled so test boots need no seed disk and no cloud-init wait.
#   Bump PROVISION_VERSION when the provisioned environment changes.

set -e

# Bump when the provisioned environment changes (packages, modules, key).
PROVISION_VERSION="3"

# --- argument parsing ---
if [ "$1" = "--provision" ]; then
    _PROVISION_MODE=1
    ARCH="$2"
    DEB_PATH=""
else
    _PROVISION_MODE=0
    ARCH="$1"
    DEB_PATH="$2"
fi

if [ -z "$ARCH" ]; then
    echo "Usage: $0 <arch> <deb_path>" >&2
    exit 1
fi
if [ "$_PROVISION_MODE" -eq 0 ] && [ -z "$DEB_PATH" ]; then
    echo "Usage: $0 <arch> <deb_path>" >&2
    exit 1
fi
if [ "$_PROVISION_MODE" -eq 0 ] && [ ! -f "$DEB_PATH" ]; then
    echo "Error: .deb file not found: $DEB_PATH" >&2
    exit 1
fi

DEB_PATH="${DEB_PATH:+$(realpath "$DEB_PATH")}"
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
CACHE_DIR="$HOME/.cache/pam_usb-qemu"
TEST_USER="tester"

mkdir -p "$CACHE_DIR"

find_bios() {
    for path in "$@"; do
        if [ -f "$path" ]; then echo "$path"; return 0; fi
    done
    return 1
}

free_port() {
    python3 -c "import socket; s=socket.socket(); s.bind(('',0)); print(s.getsockname()[1]); s.close()"
}

case "$ARCH" in
    arm64)
        QEMU_BIN="qemu-system-aarch64"
        IMAGE_URL="https://cloud-images.ubuntu.com/jammy/current/jammy-server-cloudimg-arm64.img"
        IMAGE_CACHE="${CACHE_DIR}/jammy-arm64.img"
        QEMU_MACHINE="-M virt -cpu cortex-a57 -smp 8 -m 2048"
        BIOS_PATH="$(find_bios \
            /usr/share/qemu-efi-aarch64/QEMU_EFI.fd \
            /usr/share/AAVMF/AAVMF_CODE.fd \
            /usr/share/qemu-efi/QEMU_EFI.fd)" || true
        if [ -z "$BIOS_PATH" ]; then
            echo "Error: cannot find arm64 EFI firmware" >&2; exit 1
        fi
        QEMU_BIOS="-bios ${BIOS_PATH}"
        ;;
    armhf)
        QEMU_BIN="qemu-system-arm"
        IMAGE_URL="https://cloud-images.ubuntu.com/jammy/current/jammy-server-cloudimg-armhf.img"
        IMAGE_CACHE="${CACHE_DIR}/jammy-armhf.img"
        QEMU_MACHINE="-M virt -cpu cortex-a15 -smp 2 -m 2048"
        BIOS_PATH="$(find_bios \
            /usr/share/qemu-efi-arm/QEMU_EFI.fd \
            /usr/share/qemu-efi/QEMU32_EFI.fd \
            /usr/share/AAVMF/AAVMF32_CODE.fd \
            /usr/lib/u-boot/qemu_arm/u-boot.bin)" || true
        if [ -z "$BIOS_PATH" ]; then
            echo "Error: cannot find armhf EFI/U-Boot firmware" >&2; exit 1
        fi
        QEMU_BIOS="-bios ${BIOS_PATH}"
        ;;
    *)
        echo "Unsupported arch: $ARCH (supported: arm64, armhf)" >&2
        exit 1
        ;;
esac

PROVISIONED_CACHE="${CACHE_DIR}/jammy-${ARCH}-provisioned-v${PROVISION_VERSION}.qcow2"
SSH_KEY_CACHE="${CACHE_DIR}/jammy-${ARCH}-key-v${PROVISION_VERSION}"
PROVISION_LOCK="${CACHE_DIR}/provision-${ARCH}.lock"

# --- download base cloud image ---
DOWNLOAD_LOCK="${CACHE_DIR}/download-${ARCH}.lock"
if [ ! -f "$IMAGE_CACHE" ]; then
    echo "Downloading Ubuntu Jammy cloud image for $ARCH..."
    (
        flock -x 9
        if [ ! -f "$IMAGE_CACHE" ]; then
            wget -q --show-progress -O "${IMAGE_CACHE}.tmp" "$IMAGE_URL"
            mv "${IMAGE_CACHE}.tmp" "$IMAGE_CACHE"
        fi
    ) 9>"$DOWNLOAD_LOCK"
fi

# =============================================================================
# PROVISIONING MODE
# Builds a golden qcow2 with all packages, kernel modules, and dummy_hcd
# pre-built. Embeds a fixed SSH key and disables cloud-init so test boots
# need no seed disk and start testing immediately after VM is reachable.
# =============================================================================
if [ "$_PROVISION_MODE" -eq 1 ]; then
    PROV_DIR="$(mktemp -d /tmp/pam_usb_qemu_provision_XXXXXX)"
    PROV_PIDFILE="${PROV_DIR}/qemu.pid"
    PROV_KEY="${PROV_DIR}/id_ed25519"
    PROV_PORT="$(free_port)"
    PROV_SERIAL="${PROV_DIR}/serial.log"
    PROV_QEMU_LOG="${PROV_DIR}/qemu.log"

    prov_cleanup() {
        if [ -f "$PROV_PIDFILE" ]; then
            local pid
            pid=$(cat "$PROV_PIDFILE" 2>/dev/null)
            if [ -n "$pid" ] && kill -0 "$pid" 2>/dev/null; then
                if grep -q "qemu" "/proc/$pid/cmdline" 2>/dev/null; then
                    kill "$pid" 2>/dev/null || true
                    sleep 3
                    kill -9 "$pid" 2>/dev/null || true
                fi
            fi
        fi
        rm -rf "$PROV_DIR"
        rm -f "${PROVISIONED_CACHE}.tmp" "${SSH_KEY_CACHE}.tmp"
    }
    trap prov_cleanup EXIT

    echo "=== Building pre-provisioned image for ${ARCH} (runs once per version) ==="

    PROV_DISK="${PROV_DIR}/disk.qcow2"
    qemu-img create -q -f qcow2 -b "$(realpath "$IMAGE_CACHE")" -F qcow2 "$PROV_DISK"
    qemu-img resize -q "$PROV_DISK" +8G

    # Generate the permanent SSH key that will be embedded in the golden image
    ssh-keygen -t ed25519 -N '' -f "$PROV_KEY" -C "pam_usb-qemu-${ARCH}-v${PROVISION_VERSION}" -q
    PROV_PUBKEY="$(cat "${PROV_KEY}.pub")"

    cat > "${PROV_DIR}/user-data" <<CLOUDINIT
#cloud-config
users:
  - name: ${TEST_USER}
    sudo: ALL=(ALL) NOPASSWD:ALL
    shell: /bin/bash
    ssh_authorized_keys:
      - ${PROV_PUBKEY}
packages:
  - fdisk
  - python-is-python3
  - python3-gi
  - dkms
  - gir1.2-glib-2.0
  - exfatprogs
  - git
  - make
  - gcc
  - udisks2
  - libevdev2
package_update: true
package_upgrade: false
CLOUDINIT

    cat > "${PROV_DIR}/meta-data" <<EOF
instance-id: pam-usb-provision-${ARCH}-v${PROVISION_VERSION}
local-hostname: pam-usb-provision
EOF

    PROV_SEED="${PROV_DIR}/seed.img"
    cloud-localds "$PROV_SEED" "${PROV_DIR}/user-data" "${PROV_DIR}/meta-data"

    echo "Starting provisioning VM on SSH port $PROV_PORT..."
    $QEMU_BIN \
        $QEMU_MACHINE \
        $QEMU_BIOS \
        -drive if=none,id=hd0,format=qcow2,file="$PROV_DISK" \
        -device virtio-blk-device,drive=hd0 \
        -drive if=none,id=cloud,format=raw,file="$PROV_SEED" \
        -device virtio-blk-device,drive=cloud \
        -netdev user,id=net0,hostfwd=tcp::${PROV_PORT}-:22 \
        -device virtio-net-device,netdev=net0 \
        -serial "file:${PROV_SERIAL}" \
        -D "$PROV_QEMU_LOG" \
        -display none \
        -daemonize \
        -pidfile "$PROV_PIDFILE"

    sleep 3
    if [ ! -f "$PROV_PIDFILE" ] || ! kill -0 "$(cat "$PROV_PIDFILE")" 2>/dev/null; then
        echo "Error: provisioning QEMU failed to start" >&2
        cat "$PROV_QEMU_LOG" 2>/dev/null >&2 || true
        exit 1
    fi

    PROV_SSH="ssh -o StrictHostKeyChecking=no -o UserKnownHostsFile=/dev/null -o ConnectTimeout=5 -p $PROV_PORT -i $PROV_KEY ${TEST_USER}@localhost"

    echo "Waiting for provisioning VM to boot (max 40 minutes)..."
    BOOTED=0
    for i in $(seq 1 240); do
        if ! kill -0 "$(cat "$PROV_PIDFILE")" 2>/dev/null; then
            echo "Error: provisioning QEMU died unexpectedly" >&2
            tail -80 "$PROV_SERIAL" 2>/dev/null >&2 || true
            exit 1
        fi
        if $PROV_SSH "true" 2>/dev/null; then BOOTED=1; break; fi
        if [ $((i % 12)) -eq 0 ]; then
            echo "  Still waiting... ($(( i * 10 / 60 )) min elapsed)"
        fi
        sleep 10 || true
    done

    if [ $BOOTED -eq 0 ]; then
        echo "Error: provisioning VM did not become reachable within 40 minutes" >&2
        tail -80 "$PROV_SERIAL" 2>/dev/null >&2 || true
        exit 1
    fi
    echo "Provisioning VM is reachable."

    echo "Waiting for cloud-init to finish installing packages..."
    $PROV_SSH "sudo cloud-init status --wait" || true

    echo "Installing kernel headers..."
    $PROV_SSH "sudo apt install -y linux-headers-\$(uname -r) 2>/dev/null || sudo apt install -y linux-headers-virtual"
    echo "Installing extra kernel modules (g_mass_storage etc, optional)..."
    $PROV_SSH "sudo apt install -y linux-modules-extra-\$(uname -r) 2>/dev/null || true"

    echo "Pre-building dummy_hcd kernel module..."
    $PROV_SSH "mkdir -p /tmp/src/dummy-hcd && git clone -b main https://github.com/0prichnik/dummy-hcd.git /tmp/src/dummy-hcd/master/ && cd /tmp/src/dummy-hcd/master/ && make update && make dkms"

    echo "Disabling cloud-init and apt-daily timers for test boots..."
    $PROV_SSH "sudo touch /etc/cloud/cloud-init.disabled && sudo systemctl disable --now apt-daily.timer apt-daily-upgrade.timer apt-daily.service apt-daily-upgrade.service 2>/dev/null || true"

    echo "Shutting down provisioning VM..."
    QEMU_PID="$(cat "$PROV_PIDFILE" 2>/dev/null || true)"
    $PROV_SSH "sudo shutdown -h now" 2>/dev/null || true

    for i in $(seq 1 60); do
        if [ -z "$QEMU_PID" ] || ! kill -0 "$QEMU_PID" 2>/dev/null; then break; fi
        sleep 5
    done
    if [ -n "$QEMU_PID" ] && kill -0 "$QEMU_PID" 2>/dev/null; then
        kill "$QEMU_PID" 2>/dev/null || true
        sleep 3
    fi

    if [ ! -f "$PROV_DISK" ]; then
        echo "Error: provisioned disk image missing: $PROV_DISK" >&2
        exit 1
    fi
    echo "Provisioned disk size: $(du -sh "$PROV_DISK" | cut -f1), free space: $(df -h "$CACHE_DIR" | awk 'NR==2{print $4}')"

    echo "Compressing provisioned image..."
    qemu-img convert -c -O qcow2 "$PROV_DISK" "${PROVISIONED_CACHE}.tmp" || {
        echo "Error: qemu-img convert failed (exit $?)" >&2
        exit 1
    }
    mv "${PROVISIONED_CACHE}.tmp" "$PROVISIONED_CACHE"
    cp "$PROV_KEY" "${SSH_KEY_CACHE}.tmp"
    mv "${SSH_KEY_CACHE}.tmp" "$SSH_KEY_CACHE"
    chmod 600 "$SSH_KEY_CACHE"
    echo "=== Provisioned image saved ($(du -sh "$PROVISIONED_CACHE" | cut -f1)) ==="

    trap 'rm -rf "$PROV_DIR"' EXIT
    exit 0
fi

# =============================================================================
# TEST MODE (default)
# Boots from the pre-provisioned golden image — no seed disk, no cloud-init,
# fixed SSH key. VM is ready to test as soon as sshd accepts connections.
# =============================================================================

if [ ! -f "$PROVISIONED_CACHE" ] || [ ! -f "$SSH_KEY_CACHE" ]; then
    (
        flock -x 9
        if [ ! -f "$PROVISIONED_CACHE" ] || [ ! -f "$SSH_KEY_CACHE" ]; then
            bash "$0" --provision "$ARCH"
        fi
    ) 9>"$PROVISION_LOCK"
fi

WORK_DIR="$(mktemp -d /tmp/pam_usb_qemu_XXXXXX)"
PIDFILE="${WORK_DIR}/qemu.pid"
SSH_PORT="$(free_port)"

cleanup() {
    if [ -f "$PIDFILE" ]; then
        local pid
        pid=$(cat "$PIDFILE" 2>/dev/null)
        if [ -n "$pid" ] && kill -0 "$pid" 2>/dev/null; then
            if grep -q "qemu" "/proc/$pid/cmdline" 2>/dev/null; then
                kill "$pid" 2>/dev/null || true
                for i in $(seq 1 10); do
                    if ! kill -0 "$pid" 2>/dev/null; then break; fi
                    sleep 1
                done
                kill -9 "$pid" 2>/dev/null || true
            fi
        fi
    fi
    rm -rf "$WORK_DIR"
}
trap cleanup EXIT

# CoW overlay on the pre-provisioned golden image — no seed disk needed
DISK_IMG="${WORK_DIR}/disk.qcow2"
qemu-img create -q -f qcow2 -b "$(realpath "$PROVISIONED_CACHE")" -F qcow2 "$DISK_IMG"

SERIAL_LOG="${WORK_DIR}/serial.log"
QEMU_LOG="${WORK_DIR}/qemu.log"

echo "Starting QEMU $ARCH VM on SSH port $SSH_PORT..."
$QEMU_BIN \
    $QEMU_MACHINE \
    $QEMU_BIOS \
    -drive if=none,id=hd0,format=qcow2,file="$DISK_IMG" \
    -device virtio-blk-device,drive=hd0 \
    -netdev user,id=net0,hostfwd=tcp::${SSH_PORT}-:22 \
    -device virtio-net-device,netdev=net0 \
    -serial "file:${SERIAL_LOG}" \
    -D "$QEMU_LOG" \
    -display none \
    -daemonize \
    -pidfile "$PIDFILE"

sleep 3
if [ ! -f "$PIDFILE" ] || ! kill -0 "$(cat "$PIDFILE")" 2>/dev/null; then
    echo "Error: QEMU failed to start" >&2
    cat "$QEMU_LOG" 2>/dev/null >&2 || true
    cat "$SERIAL_LOG" 2>/dev/null >&2 || true
    exit 1
fi
echo "QEMU PID: $(cat "$PIDFILE")"

SSH_CMD="ssh -o StrictHostKeyChecking=no -o UserKnownHostsFile=/dev/null -o ConnectTimeout=5 -p $SSH_PORT -i $SSH_KEY_CACHE ${TEST_USER}@localhost"
SCP_CMD="scp -o StrictHostKeyChecking=no -o UserKnownHostsFile=/dev/null -P $SSH_PORT -i $SSH_KEY_CACHE"

echo "Waiting for VM to boot (max 40 minutes)..."
BOOTED=0
for i in $(seq 1 240); do
    if ! kill -0 "$(cat "$PIDFILE")" 2>/dev/null; then
        echo "Error: QEMU process died unexpectedly" >&2
        tail -80 "$SERIAL_LOG" 2>/dev/null >&2 || true
        cat "$QEMU_LOG" 2>/dev/null >&2 || true
        exit 1
    fi
    if $SSH_CMD "true" 2>/dev/null; then
        BOOTED=1
        break
    fi
    if [ $((i % 12)) -eq 0 ]; then
        echo "  Still waiting... ($(( i * 10 / 60 )) min elapsed, last serial line: $(tail -1 "$SERIAL_LOG" 2>/dev/null | tr -d '\r' || echo '(none)'))"
    fi
    sleep 10 || true
done

if [ $BOOTED -eq 0 ]; then
    echo "Error: VM did not become reachable within 40 minutes" >&2
    tail -80 "$SERIAL_LOG" 2>/dev/null >&2 || true
    cat "$QEMU_LOG" 2>/dev/null >&2 || true
    exit 1
fi
echo "VM is reachable."

$SCP_CMD "$DEB_PATH" "${TEST_USER}@localhost:/tmp/libpam-usb.deb"
$SSH_CMD "sudo DEBIAN_FRONTEND=noninteractive apt install --reinstall -yq /tmp/libpam-usb.deb"

# Tests run inside an SSH session so deny_remote would block authentication
$SSH_CMD "sudo sed -i 's/<defaults>/<defaults><option name=\"deny_remote\">false<\/option>/g' /etc/security/pam_usb.conf"

$SSH_CMD "mkdir -p /tmp/pam_usb_tests"
$SCP_CMD -r "$SCRIPT_DIR/." "${TEST_USER}@localhost:/tmp/pam_usb_tests/"

$SSH_CMD "cd /tmp/pam_usb_tests && ./setup-dummyhcd.sh"
$SSH_CMD "cd /tmp/pam_usb_tests && ./prepare-mounting.sh"

echo "Running functional test suite in $ARCH VM..."
$SSH_CMD "cd /tmp/pam_usb_tests && ./run-tests.sh"

$SSH_CMD "sudo shutdown -h now" 2>/dev/null || true
sleep 5
