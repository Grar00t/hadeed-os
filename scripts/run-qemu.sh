#!/bin/sh
set -eu
[ "$#" -eq 1 ] || { printf '%s\n' "usage: $0 build/hadeed.img" >&2; exit 2; }
exec qemu-system-x86_64 \
  -machine pc,accel=tcg \
  -m 64M \
  -drive format=raw,file="$1",if=ide,index=0,media=disk \
  -boot c \
  -no-reboot -no-shutdown
