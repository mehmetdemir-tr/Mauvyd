# MAUVYD - An open source System manager for any linux / BSD distro

Mauvyd is a system manager which is coded in C language.


.pcg file example:

location = /path/to/program

wait = 1 **(you can remove this line if you want)**

watch = **non-used, we can delete it**


## TUTORIAL:
After creating the rootfs or debootstrap folder:

``sudo cp mauvyd ./rootfs/sbin/init
sudo cp mauvyctl ./rootfs/usr/bin/mauvyctl
sudo mkdir -p ./rootfs/etc/mauvyd
sudo mkdir -p ./rootfs/var/log/mauvyd``

### Add this pcg or your system wont be booted:

/etc/mauvyd/shell.pcg:
``
location=/bin/sh
wait=0
restart=1
interactive=1
``
/etc/mauvyd/network.pcg: (you can remove it if you dont want network in your distro)
``
location=/sbin/ifup
args=ens3
wait=1
restart=0
``
