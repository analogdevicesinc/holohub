# syntax=docker/dockerfile:1

# SPDX-FileCopyrightText: Copyright (c) 2026 Your Name / Your Organization. All rights reserved.
# SPDX-License-Identifier: Apache-2.0
#
# Development container for iio-controller.
#
# Extends the Holoscan SDK image with the build tools needed to configure,
# compile, and test the module from a live source mount. No source is copied
# into the image; the project tree is bind-mounted by the holoscan-cli at
# /workspace/iio_controller when the container is launched.
#
# Build & run via the installed Holoscan CLI:
#   holoscan run-container
#
# Manual build (without the CLI):
#   docker build -t holoscan-iio-controller .

ARG BASE_IMAGE=nvcr.io/nvidia/clara-holoscan/holoscan:v5.0.0-cuda13
FROM ${BASE_IMAGE}

ARG DEBIAN_FRONTEND=noninteractive

# Install the exact CLI pin. holoscan-cli has no runtime dependencies, so
# this adds nothing else to the image.
COPY requirements-cli.txt /tmp/requirements-cli.txt
RUN python3 -m pip install --no-cache-dir \
        --extra-index-url https://pypi.nvidia.com \
        -r /tmp/requirements-cli.txt \
    && holoscan version

# TODO: add module-specific runtime / build dependencies below.
RUN apt-get update \
    && apt-get install --no-install-recommends -y \
        libgtest-dev \
        clang-format \
    && rm -rf /var/lib/apt/lists/*
