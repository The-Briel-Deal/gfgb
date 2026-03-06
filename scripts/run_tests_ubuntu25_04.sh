#!/bin/sh

apt update
apt upgrade
apt install meson gcc libsdl3-dev cython3 python3
meson setup /build
meson test -C/build
