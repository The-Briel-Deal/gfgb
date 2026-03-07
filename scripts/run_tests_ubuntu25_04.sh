#!/bin/bash

set -e

export TZ=America/New_York
export DEBIAN_FRONTEND=noninteractive

# There are a ton of deps required for building SDL3, so I just make sure we
# populate deb-src sources so that we can use apt-get to install build-deps.
# sed -Ei 's/^Types: deb$/Types: deb deb-src/' /etc/apt/sources.list.d/ubuntu.sources

sudo apt-get -y update
sudo apt-get -y upgrade
sudo apt-get -y install gcc-14 git python3 python3-pip python3-venv

python3 -m venv /venv
source /venv/bin/activate
pip install Cython==3.2.4 pytest==8.4.2 pytest-tap==3.5 meson==1.10.1 meson-python==0.19.0

meson setup /build -Dpython_install=/venv/bin/python
meson test -C/build --verbose
