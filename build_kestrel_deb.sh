#!/usr/bin/env bash
set -euo pipefail

VERSION="1.0.0"
ARCH="amd64"
PKG_NAME="chronos-db"
BUILD_DIR="$HOME/chronos-db/deb_build"
PKGROOT="$BUILD_DIR/pkgroot"

echo "== Cleaning previous build =="
rm -rf "$BUILD_DIR"
mkdir -p "$PKGROOT/DEBIAN" "$PKGROOT/usr/bin" "$PKGROOT/lib/systemd/system"

echo "== Compiling binaries from ~/chronos-db source tree =="
cd ~/chronos-db
gcc -O2 -std=gnu99 -pthread -I include \
    -o "$PKGROOT/usr/bin/chronos_main" \
    src/main.c src/chronos.c src/election.c src/ares_bft.c -lcrypto
gcc -O2 -std=gnu99 -pthread -I include \
    -o "$PKGROOT/usr/bin/chronos_query" \
    tools/chronos_query.c src/chronos.c -lcrypto
chmod 755 "$PKGROOT/usr/bin/chronos_main" "$PKGROOT/usr/bin/chronos_query"
echo "   Built OK"

echo "== Writing systemd unit =="
cat > "$PKGROOT/lib/systemd/system/chronos-db@.service" << 'EOF'
[Unit]
Description=Chronos-DB Telemetry Logging Service (instance: %i)
After=network.target

[Service]
Type=simple
User=chronos-svc
Group=chronos-svc
Environment=CHRONOS_INSTANCE_NAME=%i
Environment=CHRONOS_DB_PATH=/var/lib/chronos/chronos_telemetry.db
Environment=CHRONOS_LOCK_PATH=/var/lib/chronos/election.lock
ExecStart=/usr/bin/chronos_main
WorkingDirectory=/var/lib/chronos
Restart=on-failure
RestartSec=2
SyslogIdentifier=chronos-db-%i
NoNewPrivileges=true
ProtectSystem=strict
ProtectHome=true
ReadWritePaths=/var/lib/chronos

[Install]
WantedBy=multi-user.target
EOF

echo "== Writing control file =="
cat > "$PKGROOT/DEBIAN/control" << EOF
Package: $PKG_NAME
Version: $VERSION
Section: admin
Priority: optional
Architecture: $ARCH
Depends: libssl3 | libssl3t64 | libssl1.1
Maintainer: Kestrel OS Team <root@localhost>
Description: Chronos-DB tamper-evident telemetry logging service
 Memory-mapped, hash-chained time-series logging engine with
 automatic active/passive failover across multiple instances,
 and Ares-BFT Merkle-tree based log integrity verification.
 Ships as a systemd template unit (chronos-db@.service) so
 multiple standby instances can run, contending for a single
 election lock; only one is ever active at a time.
EOF

echo "== Writing postinst (chroot-safe: enables units even without a running systemd, only starts them on a real boot) =="
cat > "$PKGROOT/DEBIAN/postinst" << 'EOF'
#!/bin/sh
set -e

SERVICE_USER="chronos-svc"
DATA_DIR="/var/lib/chronos"
INSTANCES="primary backup1 backup2"

case "$1" in
    configure)
        if ! getent passwd "$SERVICE_USER" >/dev/null 2>&1; then
            adduser --system --no-create-home --shell /usr/sbin/nologin \
                --group "$SERVICE_USER"
        fi

        mkdir -p "$DATA_DIR"
        chown "$SERVICE_USER:$SERVICE_USER" "$DATA_DIR"
        chmod 700 "$DATA_DIR"

        if command -v systemctl >/dev/null 2>&1; then
            systemctl daemon-reload || true
            for name in $INSTANCES; do
                systemctl enable "chronos-db@${name}.service" || true
            done
        fi

        if [ -d /run/systemd/system ]; then
            for name in $INSTANCES; do
                systemctl start "chronos-db@${name}.service" || true
            done
        fi
        ;;
esac

exit 0
EOF

echo "== Writing prerm =="
cat > "$PKGROOT/DEBIAN/prerm" << 'EOF'
#!/bin/sh
set -e

INSTANCES="primary backup1 backup2"

case "$1" in
    remove|upgrade|deconfigure)
        if command -v systemctl >/dev/null 2>&1 && [ -d /run/systemd/system ]; then
            for name in $INSTANCES; do
                systemctl stop "chronos-db@${name}.service" || true
            done
        fi
        ;;
esac

exit 0
EOF

echo "== Writing postrm (data preserved on remove; purge disables units but does NOT delete data by default) =="
cat > "$PKGROOT/DEBIAN/postrm" << 'EOF'
#!/bin/sh
set -e

INSTANCES="primary backup1 backup2"

case "$1" in
    purge)
        if command -v systemctl >/dev/null 2>&1; then
            for name in $INSTANCES; do
                systemctl disable "chronos-db@${name}.service" >/dev/null 2>&1 || true
            done
            systemctl daemon-reload || true
        fi
        # Data intentionally preserved -- uncomment to wipe on purge:
        # rm -rf /var/lib/chronos
        # deluser --system chronos-svc >/dev/null 2>&1 || true
        ;;
esac

exit 0
EOF

chmod 755 "$PKGROOT/DEBIAN/postinst" "$PKGROOT/DEBIAN/prerm" "$PKGROOT/DEBIAN/postrm"

echo "== Building .deb =="
cd "$BUILD_DIR"
dpkg-deb --build --root-owner-group pkgroot "${PKG_NAME}_${VERSION}_${ARCH}.deb"

echo ""
echo "== Done =="
echo "Package: $BUILD_DIR/${PKG_NAME}_${VERSION}_${ARCH}.deb"
echo ""
echo "Install it:"
echo "  sudo dpkg -i $BUILD_DIR/${PKG_NAME}_${VERSION}_${ARCH}.deb"
echo ""
echo "This same file can be included directly in a Kestrel OS image build"
echo "(debootstrap --include, live-build package list, etc) -- it installs"
echo "cleanly even with no systemd running, and self-enables for first boot."
