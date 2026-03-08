#!/bin/bash

set -ev

echo XDG_RUNTIME_DIR = $XDG_RUNTIME_DIR

export TZ=America/New_York
export DEBIAN_FRONTEND=noninteractive

cd /gb_emu/

# There are a ton of deps required for building SDL3, so I just make sure we
# populate deb-src sources so that we can use apt-get to install build-deps.
sed -Ei 's/^Types: deb$/Types: deb deb-src/' /etc/apt/sources.list.d/ubuntu.sources

apt-get -y update
apt-get -y upgrade
apt-get -y install meson gcc git python3 python3-pip python3-venv
apt-get -y build-dep libsdl3-dev

python3 -m venv /venv
source /venv/bin/activate
pip install Cython==3.2.4 pytest==8.4.2 pytest-tap==3.5

meson setup /build -Dpython_install=/venv/bin/python
meson test -C/build --verbose
