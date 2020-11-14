#!/bin/sh

set -eu

rm -rf -- check_fstab
mkdir -p -- check_fstab

cat <<EOF > check_fstab/fstab
check_fstab/a /foo btrfs "" 0 0
check_fstab/e /foo btrfs "" 0 0
EOF

(
    cd check_fstab || exit 1
    touch a
    ln -s a b
    touch e
    ln -s e d
    ln -s d c
)

"$1"
