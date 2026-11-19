# XFOIL.next portable Linux builder
#
# Copyright (C) 2026 Olaf Frommann
# https://github.com/of33/XFOIL.next
#
# SPDX-License-Identifier: GPL-3.0-or-later
#
# Pinned Ubuntu 22.04 / GCC 13.4.0 / glibc 2.35 builder for the
# single-ELF portable runtime. The base image is pinned by digest.
# The PPA package versions are pinned where available; the produced
# image ID and package inventory are recorded in the B1 report.

FROM ubuntu:22.04@sha256:9abcaffc92a5279fff3a6a41635c0800ea33e528425fb389658806476e7defde

ENV DEBIAN_FRONTEND=noninteractive

RUN apt-get update \
 && apt-get install -y --no-install-recommends \
      software-properties-common gnupg ca-certificates \
 && add-apt-repository -y ppa:ubuntu-toolchain-r/test \
 && apt-get update \
 && apt-get install -y --no-install-recommends \
      g++-13=13.4.0-6ubuntu1~22~ppa2 \
      gcc-13=13.4.0-6ubuntu1~22~ppa2 \
      cpp-13=13.4.0-6ubuntu1~22~ppa2 \
      libstdc++-13-dev=13.4.0-6ubuntu1~22~ppa2 \
      libgcc-13-dev=13.4.0-6ubuntu1~22~ppa2 \
      gcc-13-base=13.4.0-6ubuntu1~22~ppa2 \
      make binutils file ca-certificates \
 && rm -rf /var/lib/apt/lists/*

LABEL org.opencontainers.image.title="XFOIL.next portable Linux builder"
LABEL org.opencontainers.image.version="ubuntu-22.04-gcc-13.4.0-glibc-2.35"
LABEL org.opencontainers.image.description="Pinned Ubuntu 22.04 x86_64 builder for XFOIL.next portable Linux binary"

WORKDIR /src
