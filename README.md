nat26
=====

This is an OpenWRT feed with a Linux kernel module implementing hacky NAT26.

Compiling
=========

With Barrier Breaker (trunk), add the following line to *feeds.conf.default*:
```
src-git nat26 https://github.com/codefetch/nat46.git
```

then issue:

```
./scripts/feeds update -a
./scripts/feeds install -a -p nat26
```

This will cause the following to appear in the "make menuconfig":

 * Kernel modules -> Network Support -> kmod-nat26

Managing
========

The management of the NAT26 interfaces is done via the /proc/net/nat26/control file.

For more information about the module, take a look at the nat26/modules/README file.


