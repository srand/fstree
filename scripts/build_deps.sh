#!/bin/bash

set -e

# If running on Alpine:
if [ -f /etc/alpine-release ]; then
    apk add --no-cache gcompat perl

    # Install dependencies for building libfstree and its dependencies
    /opt/python/cp312-cp312/bin/pip install jolt

    # Build
    rm -rf sysroot
    /opt/python/cp312-cp312/bin/python -m jolt -vvv -c jolt.cachedir=/host/tmp/musl-cache build fstree/deps -c sysroot/
else 
    # Install build dependencies for RHEL
    yum install -y perl

    # Install dependencies for building libfstree and its dependencies
    /opt/python/cp312-cp312/bin/pip install jolt

    # Build 
    rm -rf sysroot
    /opt/python/cp312-cp312/bin/python -m jolt -vvv -c jolt.cachedir=/host/tmp/rhel-cache build fstree/deps -c sysroot/
fi
