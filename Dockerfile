# hadolint ignore=DL3007
FROM archlinux:latest

WORKDIR /app

COPY . .

RUN set -eux; \
  pacman -Syy --noconfirm \
  arm-none-eabi-gcc \
  arm-none-eabi-newlib \
  base-devel \
  git

RUN git submodule update --init --recursive
#RUN make && cp firmware* compiled-firmware/
