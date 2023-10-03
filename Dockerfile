# hadolint ignore=DL3007
FROM archlinux:latest

RUN set -eux; \
  pacman -Syy --noconfirm \
  arm-none-eabi-gcc \
  arm-none-eabi-newlib \
  base-devel \
  git \
  python-crcmod

WORKDIR /app

COPY . .

RUN git submodule update --init --recursive
