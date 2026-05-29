# MAUVYD - An Open Source System Manager for Linux/BSD

MAUVYD is a minimal system manager written in C.

---

## .pcg File Format

```
location=/path/to/program
args=arg1 arg2
wait=1
restart=1
interactive=1
depends=other_service
```

`wait` and `args` are optional and can be omitted.

---

## Installation

After creating your rootfs or debootstrap folder:

```sh
sudo cp mauvyd ./rootfs/sbin/init
sudo cp mauvyctl ./rootfs/usr/bin/mauvyctl
sudo mkdir -p ./rootfs/etc/mauvyd
sudo mkdir -p ./rootfs/var/log/mauvyd
```

---

## Required Services

Without this file your system **will not boot:**

`/etc/mauvyd/shell.pcg`
```
location=/bin/sh
wait=0
restart=1
interactive=1
```

---

## Optional Services

`/etc/mauvyd/network.pcg` — remove if you don't need networking:
```
location=/sbin/ifup
args=ens3
wait=1
restart=0
```
