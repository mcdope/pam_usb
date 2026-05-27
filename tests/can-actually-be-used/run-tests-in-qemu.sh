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

set -e

ARCH="$1"
DEB_PATH="$2"

if [ -z "$ARCH" ] || [ -z "$DEB_PATH" ]; then
    echo "Usage: $0 <arch> <deb_path>" >&2
    exit 1
fi

if [ ! -f "$DEB_PATH" ]; then
    echo "Error: .deb file not found: $DEB_PATH" >&2
    exit 1
fi

DEB_PATH="$(realpath "$DEB_PATH")"
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
CACHE_DIR="$HOME/.cache/pam_usb-qemu"
WORK_DIR="$(mktemp -d /tmp/pam_usb_qemu_XXXXXX)"
SSH_KEY="${WORK_DIR}/id_ed25519"
PIDFILE="${WORK_DIR}/qemu.pid"
TEST_USER="tester"

mkdir -p "$CACHE_DIR"

cleanup() {
    if [ -f "$PIDFILE" ]; then
        local pid
        pid=$(cat "$PIDFILE")
        if kill -0 "$pid" 2>/dev/null; then
            kill "$pid" 2>/dev/null || true
            for i in $(seq 1 10); do
                if ! kill -0 "$pid" 2>/dev/null; then
                    break
                fi
                sleep 1
            done
            if kill -0 "$pid" 2>/dev/null; then
                kill -9 "$pid" 2>/dev/null || true
            fi
        fi
    fi
    rm -rf "$WORK_DIR"
}
trap cleanup EXIT

SSH_PORT=$(python3 -c "import socket; s=socket.socket(); s.bind(('',0)); print(s.getsockname()[1]); s.close()")

case "$ARCH" in
    arm64)
        QEMU_BIN="qemu-system-aarch64"
        IMAGE_URL="https://cloud-images.ubuntu.com/jammy/current/jammy-server-cloudimg-arm64.img"
        IMAGE_CACHE="${CACHE_DIR}/jammy-arm64.img"
        QEMU_MACHINE="-M virt -cpu cortex-a57 -m 2048"
        QEMU_BIOS="-bios /usr/share/qemu-efi-aarch64/QEMU_EFI.fd"
        ;;
    armhf)
        QEMU_BIN="qemu-system-arm"
        IMAGE_URL="https://cloud-images.ubuntu.com/jammy/current/jammy-server-cloudimg-armhf.img"
        IMAGE_CACHE="${CACHE_DIR}/jammy-armhf.img"
        QEMU_MACHINE="-M virt -cpu cortex-a15 -m 2048"
        QEMU_BIOS="-bios /usr/share/qemu-efi-arm/QEMU_EFI.fd"
        ;;
    *)
        echo "Unsupported arch: $ARCH (supported: arm64, armhf)" >&2
        exit 1
        ;;
esac

if [ ! -f "$IMAGE_CACHE" ]; then
    echo "Downloading Ubuntu Jammy cloud image for $ARCH..."
    wget -q --show-progress -O "${IMAGE_CACHE}.tmp" "$IMAGE_URL"
    mv "${IMAGE_CACHE}.tmp" "$IMAGE_CACHE"
fi

DISK_IMG="${WORK_DIR}/disk.qcow2"
qemu-img create -q -f qcow2 -b "$(realpath "$IMAGE_CACHE")" -F qcow2 "$DISK_IMG"
qemu-img resize -q "$DISK_IMG" +8G

ssh-keygen -t ed25519 -N '' -f "$SSH_KEY" -C 'pam_usb-qemu-test' -q
PUBKEY="$(cat "${SSH_KEY}.pub")"

cat > "${WORK_DIR}/user-data" <<CLOUDINIT
#cloud-config
users:
  - name: ${TEST_USER}
    sudo: ALL=(ALL) NOPASSWD:ALL
    shell: /bin/bash
    ssh_authorized_keys:
      - ${PUBKEY}
packages:
  - fdisk
  - libudisks2-dev
  - libxml2-dev
  - python-is-python3
  - python3-gi
  - python3-dotenv
  - libpam0g-dev
  - devscripts
  - debhelper
  - dkms
  - pkg-config
  - gir1.2-glib-2.0
  - libglib2.0-dev
  - libevdev-dev
  - exfatprogs
  - git
  - make
  - gcc
  - wget
package_update: true
package_upgrade: false
CLOUDINIT

cat > "${WORK_DIR}/meta-data" <<EOF
instance-id: pam-usb-qemu-test-${ARCH}
local-hostname: pam-usb-qemu
EOF

SEED_IMG="${WORK_DIR}/seed.img"
cloud-localds "$SEED_IMG" "${WORK_DIR}/user-data" "${WORK_DIR}/meta-data"

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
    -display none \
    -daemonize \
    -pidfile "$PIDFILE"

SSH_CMD="ssh -o StrictHostKeyChecking=no -o UserKnownHostsFile=/dev/null -o ConnectTimeout=5 -p $SSH_PORT -i $SSH_KEY ${TEST_USER}@localhost"
SCP_CMD="scp -o StrictHostKeyChecking=no -o UserKnownHostsFile=/dev/null -P $SSH_PORT -i $SSH_KEY"

echo "Waiting for VM to boot (max 10 minutes)..."
BOOTED=0
for i in $(seq 1 60); do
    if $SSH_CMD "true" 2>/dev/null; then
        BOOTED=1
        break
    fi
    sleep 10 || true
done

if [ $BOOTED -eq 0 ]; then
    echo "Error: VM did not become reachable within 10 minutes" >&2
    exit 1
fi
echo "VM is reachable."

echo "Waiting for cloud-init to complete package installation..."
$SSH_CMD "sudo cloud-init status --wait" || true

# Install kernel headers matching the booted kernel for dummy-hcd DKMS build
$SSH_CMD "sudo apt install -y linux-headers-\$(uname -r) 2>/dev/null || sudo apt install -y linux-headers-virtual"

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
