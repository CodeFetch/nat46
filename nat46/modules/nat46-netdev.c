/*
 * Network device related boilerplate functions
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



#include <linux/if_arp.h>
#include <linux/netdevice.h>
#include <linux/route.h>
#include <linux/skbuff.h>
#include <net/ip6_fib.h>
#include <net/ip6_route.h>
#include <net/ipv6.h>
#include <linux/version.h>
#include "nat26-core.h"
#include "nat26-module.h"

#define NETDEV_DEFAULT_NAME "nat26."

typedef struct {
  u32 sig;
  nat26_instance_t *nat26;
} nat26_netdev_priv_t;

static u8 netdev_count = 0;

static int nat26_netdev_up(struct net_device *dev);
static int nat26_netdev_down(struct net_device *dev);

static netdev_tx_t nat26_netdev_xmit(struct sk_buff *skb, struct net_device *dev);


static const struct net_device_ops nat26_netdev_ops = {
	.ndo_open       = nat26_netdev_up,      /* Called at ifconfig nat26 up */
	.ndo_stop       = nat26_netdev_down,    /* Called at ifconfig nat26 down */
	.ndo_start_xmit = nat26_netdev_xmit,    /* REQUIRED, must return NETDEV_TX_OK */
};

static int nat26_netdev_up(struct net_device *dev)
{
	netif_start_queue(dev);
	return 0;
}

static int nat26_netdev_down(struct net_device *dev)
{
	netif_stop_queue(dev);
	return 0;
}

static netdev_tx_t nat26_netdev_xmit(struct sk_buff *skb, struct net_device *dev)
{
	dev->stats.rx_packets++;
	dev->stats.rx_bytes += skb->len;
	if(ETH_P_IP == ntohs(skb->protocol)) {
		nat26_ipv4_input(skb);
	}
	if(ETH_P_IPV6 == ntohs(skb->protocol)) {
		nat26_ipv6_input(skb);
	}
	kfree_skb(skb);
	return NETDEV_TX_OK;
}

void nat26_netdev_count_xmit(struct sk_buff *skb, struct net_device *dev) {
	dev->stats.tx_packets++;
	dev->stats.tx_bytes += skb->len;
}

void *netdev_nat26_instance(struct net_device *dev) {
	nat26_netdev_priv_t *priv = netdev_priv(dev);
	return priv->nat26;
}

static void netdev_nat26_set_instance(struct net_device *dev, nat26_instance_t *new_nat26) {
	nat26_netdev_priv_t *priv = netdev_priv(dev);
	if(priv->nat26) {
		release_nat26_instance(priv->nat26);
	}
	priv->nat26 = new_nat26;
}

void nat26_netdev_set_priv(struct net_device *dev, void *nat26)
{
	nat26_netdev_priv_t *priv = netdev_priv(dev);
	memset(priv, 0, sizeof(*priv));
	priv->sig = NAT26_DEVICE_SIGNATURE;
	priv->nat26 = (nat26_instance_t*)nat26;
}

static void nat26_netdev_setup(struct net_device *dev)
{
	nat26_instance_t *nat26 = alloc_nat26_instance(1, NULL, -1, -1);

	nat26_netdev_set_priv(dev, nat26);

	dev->netdev_ops = &nat26_netdev_ops;
	dev->type = ARPHRD_NONE;
	dev->hard_header_len = 0;
	dev->addr_len = 0;
	dev->mtu = 16384; /* iptables does reassembly. Rather than using ETH_DATA_LEN, let's try to get as much mileage as we can with the Linux stack */
	dev->features = NETIF_F_NETNS_LOCAL;
	dev->flags = IFF_NOARP | IFF_POINTOPOINT;
}

int nat26_netdev_create(char *basename, struct net_device **dev)
{
	int ret = 0;
	char *devname = NULL;
	int automatic_name = 0;

	if (basename && strcmp("", basename)) {
		devname = kmalloc(strlen(basename)+1, GFP_KERNEL);
	} else {
		devname = kmalloc(strlen(NETDEV_DEFAULT_NAME)+3+1, GFP_KERNEL);
		automatic_name = 1;
	}
	if (!devname) {
		printk("nat26: can not allocate memory to store device name.\n");
		ret = -ENOMEM;
		goto err;
	}
	if (automatic_name) {
		snprintf(devname, strlen(NETDEV_DEFAULT_NAME)+3, "%s%d", NETDEV_DEFAULT_NAME, netdev_count);
		netdev_count++;
	} else {
		strcpy(devname, basename);
	}

#if LINUX_VERSION_CODE <= KERNEL_VERSION(3,17,0)
	*dev = alloc_netdev(sizeof(nat26_instance_t), devname, nat26_netdev_setup);
#else
	*dev = alloc_netdev(sizeof(nat26_instance_t), devname, NET_NAME_UNKNOWN, nat26_netdev_setup);
#endif
	if (!*dev) {
		printk("nat26: Unable to allocate nat26 device '%s'.\n", devname);
		ret = -ENOMEM;
		goto err_alloc_dev;
	}

	ret = register_netdev(*dev);
	if(ret) {
		printk("nat26: Unable to register nat26 device.\n");
		ret = -ENOMEM;
		goto err_register_dev;
	}

	printk("nat26: netdevice nat26 '%s' created successfully.\n", devname);
	kfree(devname);


err_register_dev:
	free_netdev(*dev);
err_alloc_dev:
	kfree(devname);
err:
	return ret;
}

void nat26_netdev_destroy(struct net_device *dev)
{
	nat26_instance_t *nat26 = netdev_nat26_instance(dev);

	if(nat26->dev2 == dev)  {
	  printk("nat26: Destroying nat26 L3 device.\n");
	  unregister_netdev(nat26->dev6);
	} else {
	  printk("nat26: Destroying nat26 L2 device.\n");
	  unregister_netdev(nat26->dev2);
	}
	netdev_nat26_set_instance(dev, NULL);
	printk("nat26: Destroying nat26 device.\n");
	unregister_netdev(dev);
}

static int is_nat26(struct net_device *dev) {
	nat26_netdev_priv_t *priv = netdev_priv(dev);
	return (priv && (NAT26_DEVICE_SIGNATURE == priv->sig));
}


static struct net_device *find_dev(char *name) {
	struct net_device *dev;
	struct net_device *out = NULL;

	if(!name) {
		return NULL;
	}

	read_lock(&dev_base_lock);
	dev = first_net_device(&init_net);
	while (dev) {
		if((0 == strcmp(dev->name, name)) && is_nat26(dev)) {
			if(debug) {
				printk(KERN_INFO "found [%s]\n", dev->name);
			}
			out = dev;
			break;
		}
		dev = next_net_device(dev);
	}
	read_unlock(&dev_base_lock);
	return out;
}

void *nat26_find_dev(char *devname) {
	struct net_device *dev;
	dev = find_dev(devname);
	return dev;
}

int nat26_create(char *devname) {
	int ret = 0;
	struct net_device *dev = find_dev(devname);
	if (dev) {
		printk("Can not add: device '%s' already exists!\n", devname);
		return -1;
	}
	ret = nat26_netdev_create(devname, &dev);
	return ret;
}

int nat26_destroy(char *devname) {
	struct net_device *dev = find_dev(devname);
	if(dev) {
		printk("Destroying '%s'\n", devname);
		nat26_netdev_destroy(dev);
		return 0;
	} else {
		printk("Could not find device '%s'\n", devname);
		return -1;
	}
}

int nat26_insert(char *devname, char *buf) {
	struct net_device *dev = find_dev(devname);
	int ret = -1;
	if(dev) {
		nat26_instance_t *nat26 = netdev_nat26_instance(dev);
		nat26_instance_t *nat26_new = alloc_nat26_instance(nat26->npairs+1, nat26, 0, 1);
		if(nat26_new) {
			netdev_nat26_set_instance(dev, nat26_new);
			ret = nat26_set_ipair_config(nat26_new, 0, buf, strlen(buf));
		} else {
			printk("Could not insert a new rule on device %s\n", devname);
		}
	}
	return ret;
}

int nat26_configure(char *devname, char *buf) {
	struct net_device *dev = find_dev(devname);
	if(dev) {
		nat26_instance_t *nat26 = netdev_nat26_instance(dev);
		return nat26_set_config(nat26, buf, strlen(buf));
	} else {
		return -1;
	}
}

void nat64_show_all_configs(struct seq_file *m) {
        struct net_device *dev;
	read_lock(&dev_base_lock);
	dev = first_net_device(&init_net);
	while (dev) {
		if(is_nat26(dev)) {
			nat26_instance_t *nat26 = netdev_nat26_instance(dev);
			int buflen = 1024;
			int ipair = -1;
			char *buf = kmalloc(buflen+1, GFP_KERNEL);
			seq_printf(m, "add %s\n", dev->name);
			if(buf) {
				for(ipair = 0; ipair < nat26->npairs; ipair++) {
					nat26_get_ipair_config(nat26, ipair, buf, buflen);
					if(ipair < nat26->npairs-1) {
						seq_printf(m,"insert %s %s\n", dev->name, buf);
					} else {
						seq_printf(m,"config %s %s\n", dev->name, buf);
					}
				}
				seq_printf(m,"\n");
				kfree(buf);
			}
		}
		dev = next_net_device(dev);
	}
	read_unlock(&dev_base_lock);

}

void nat26_destroy_all(void) {
        struct net_device *dev;
        struct net_device *nat26dev;
	do {
		read_lock(&dev_base_lock);
		nat26dev = NULL;
		dev = first_net_device(&init_net);
		while (dev) {
			if(is_nat26(dev)) {
				nat26dev = dev;
			}
			dev = next_net_device(dev);
		}
		read_unlock(&dev_base_lock);
		if(nat26dev) {
			nat26_netdev_destroy(nat26dev);
		}
	} while (nat26dev);

}
