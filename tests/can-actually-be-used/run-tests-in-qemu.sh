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
# On first run for a given arch, a pre-provisioned golden image is built and
# cached in ~/.cache/pam_usb-qemu/.  Subsequent runs skip apt/cloud-init
# package installation and boot directly from the golden image.  Bump
# PROVISION_VERSION when the package list changes to force a rebuild.

set -e

# Bump when the provisioned package list changes to force a rebuild.
PROVISION_VERSION="1"

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
        QEMU_MACHINE="-M virt -cpu cortex-a15 -smp 8 -m 2048"
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
PROVISION_LOCK="${CACHE_DIR}/provision-${ARCH}.lock"

# --- download base cloud image (shared by both modes) ---
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
# Builds a golden qcow2 with all packages pre-installed and saves it to cache.
# Triggered automatically from test mode when PROVISIONED_CACHE is missing.
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
            pid=$(cat "$PROV_PIDFILE")
            kill "$pid" 2>/dev/null || true
            sleep 3
            kill -9 "$pid" 2>/dev/null || true
        fi
        rm -rf "$PROV_DIR"
        rm -f "${PROVISIONED_CACHE}.tmp"
    }
    trap prov_cleanup EXIT

    echo "=== Building pre-provisioned image for ${ARCH} (runs once per version) ==="

    PROV_DISK="${PROV_DIR}/disk.qcow2"
    qemu-img create -q -f qcow2 -b "$(realpath "$IMAGE_CACHE")" -F qcow2 "$PROV_DISK"
    qemu-img resize -q "$PROV_DISK" +8G

    ssh-keygen -t ed25519 -N '' -f "$PROV_KEY" -C 'pam_usb-qemu-provision' -q
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
package_update: true
package_upgrade: false
CLOUDINIT

    cat > "${PROV_DIR}/meta-data" <<EOF
instance-id: pam-usb-provision-${ARCH}
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

    echo "Resetting cloud-init state so it runs fresh on next boot..."
    $PROV_SSH "sudo cloud-init clean --logs"

    echo "Shutting down provisioning VM..."
    $PROV_SSH "sudo shutdown -h now" 2>/dev/null || true

    # Wait for QEMU to exit on its own (graceful VM shutdown)
    for i in $(seq 1 60); do
        if ! kill -0 "$(cat "$PROV_PIDFILE")" 2>/dev/null; then break; fi
        sleep 5
    done
    # Force kill if still running
    if kill -0 "$(cat "$PROV_PIDFILE")" 2>/dev/null; then
        kill "$(cat "$PROV_PIDFILE")" 2>/dev/null || true
        sleep 3
    fi

    echo "Compressing provisioned image to ${PROVISIONED_CACHE}..."
    qemu-img convert -c -O qcow2 "$PROV_DISK" "${PROVISIONED_CACHE}.tmp"
    mv "${PROVISIONED_CACHE}.tmp" "$PROVISIONED_CACHE"
    echo "=== Provisioned image saved ($(du -sh "$PROVISIONED_CACHE" | cut -f1)) ==="

    # Disarm the cleanup trap — image is saved, temp dir will be removed normally
    trap 'rm -rf "$PROV_DIR"' EXIT
    exit 0
fi

# =============================================================================
# TEST MODE (default)
# Uses the pre-provisioned golden image; cloud-init only injects the SSH key.
# =============================================================================

# Ensure provisioned image exists (build it if not, serialised with flock)
if [ ! -f "$PROVISIONED_CACHE" ]; then
    (
        flock -x 9
        if [ ! -f "$PROVISIONED_CACHE" ]; then
            bash "$0" --provision "$ARCH"
        fi
    ) 9>"$PROVISION_LOCK"
fi

WORK_DIR="$(mktemp -d /tmp/pam_usb_qemu_XXXXXX)"
SSH_KEY="${WORK_DIR}/id_ed25519"
PIDFILE="${WORK_DIR}/qemu.pid"
SSH_PORT="$(free_port)"

cleanup() {
    if [ -f "$PIDFILE" ]; then
        local pid
        pid=$(cat "$PIDFILE")
        if kill -0 "$pid" 2>/dev/null; then
            kill "$pid" 2>/dev/null || true
            for i in $(seq 1 10); do
                if ! kill -0 "$pid" 2>/dev/null; then break; fi
                sleep 1
            done
            kill -9 "$pid" 2>/dev/null || true
        fi
    fi
    rm -rf "$WORK_DIR"
}
trap cleanup EXIT

# Create CoW overlay on top of the pre-provisioned golden image
DISK_IMG="${WORK_DIR}/disk.qcow2"
qemu-img create -q -f qcow2 -b "$(realpath "$PROVISIONED_CACHE")" -F qcow2 "$DISK_IMG"

ssh-keygen -t ed25519 -N '' -f "$SSH_KEY" -C 'pam_usb-qemu-test' -q
PUBKEY="$(cat "${SSH_KEY}.pub")"

# Minimal cloud-init: user + SSH key injection only — no package installation
cat > "${WORK_DIR}/user-data" <<CLOUDINIT
#cloud-config
users:
  - name: ${TEST_USER}
    sudo: ALL=(ALL) NOPASSWD:ALL
    shell: /bin/bash
    ssh_authorized_keys:
      - ${PUBKEY}
package_update: false
package_upgrade: false
CLOUDINIT

cat > "${WORK_DIR}/meta-data" <<EOF
instance-id: pam-usb-qemu-test-${ARCH}-$$
local-hostname: pam-usb-qemu
EOF

SEED_IMG="${WORK_DIR}/seed.img"
cloud-localds "$SEED_IMG" "${WORK_DIR}/user-data" "${WORK_DIR}/meta-data"

SERIAL_LOG="${WORK_DIR}/serial.log"
QEMU_LOG="${WORK_DIR}/qemu.log"

echo "Starting QEMU $ARCH VM on SSH port $SSH_PORT..."
$QEMU_BIN \
    $QEMU_MACHINE \
    $QEMU_BIOS \
    -drive if=none,id=hd0,format=qcow2,file="$DISK_IMG" \
    -device virtio-blk-device,drive=hd0 \
    -drive if=none,id=cloud,format=raw,file="$SEED_IMG" \
    -device virtio-blk-device,drive=cloud \
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
    echo "--- QEMU log ---" >&2
    cat "$QEMU_LOG" 2>/dev/null >&2 || true
    echo "--- Serial output ---" >&2
    cat "$SERIAL_LOG" 2>/dev/null >&2 || true
    exit 1
fi
echo "QEMU PID: $(cat "$PIDFILE")"

SSH_CMD="ssh -o StrictHostKeyChecking=no -o UserKnownHostsFile=/dev/null -o ConnectTimeout=5 -p $SSH_PORT -i $SSH_KEY ${TEST_USER}@localhost"
SCP_CMD="scp -o StrictHostKeyChecking=no -o UserKnownHostsFile=/dev/null -P $SSH_PORT -i $SSH_KEY"

echo "Waiting for VM to boot (max 40 minutes)..."
BOOTED=0
for i in $(seq 1 240); do
    if ! kill -0 "$(cat "$PIDFILE")" 2>/dev/null; then
        echo "Error: QEMU process died unexpectedly" >&2
        echo "--- Last 80 lines of serial console ---" >&2
        tail -80 "$SERIAL_LOG" 2>/dev/null >&2 || true
        echo "--- QEMU log ---" >&2
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
    echo "--- Last 80 lines of serial console ---" >&2
    tail -80 "$SERIAL_LOG" 2>/dev/null >&2 || true
    echo "--- QEMU log ---" >&2
    cat "$QEMU_LOG" 2>/dev/null >&2 || true
    exit 1
fi
echo "VM is reachable."

# Cloud-init only injects the SSH key now — should complete in under 2 minutes
echo "Waiting for cloud-init (SSH key injection only)..."
$SSH_CMD "sudo cloud-init status --wait" || true

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
