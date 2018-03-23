/*
 *
 * Copyright (c) 2013-2014 Andrew Yourtchenko <ayourtch@gmail.com>
 * Copyright (c) 2018 Vincent Wiemann <vincent.wiemann@ironai.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2
 * as published by the Free Software Foundation
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 */

#define NAT26_DEVICE_SIGNATURE 0x544e36de


int nat26_create(char *devname);
int nat26_destroy(char *devname);
int nat26_insert(char *devname, char *buf);
int nat26_configure(char *devname, char *buf);
void nat26_destroy_all(void);
void nat64_show_all_configs(struct seq_file *m);
void nat26_netdev_set_priv(struct net_device *dev, void *nat26);
void nat26_netdev_count_xmit(struct sk_buff *skb, struct net_device *dev);
void *nat26_find_dev(char *devname);
void *netdev_nat26_instance(struct net_device *dev);

