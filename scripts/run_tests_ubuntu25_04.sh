#!/bin/sh

apt update
apt upgrade
apt install meson gcc cython3 python3
meson setup /build
meson test -C/build
