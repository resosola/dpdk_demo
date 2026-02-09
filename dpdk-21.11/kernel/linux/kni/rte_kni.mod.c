#include <linux/module.h>
#define INCLUDE_VERMAGIC
#include <linux/build-salt.h>
#include <linux/elfnote-lto.h>
#include <linux/vermagic.h>
#include <linux/compiler.h>

BUILD_SALT;
BUILD_LTO_INFO;

MODULE_INFO(vermagic, VERMAGIC_STRING);
MODULE_INFO(name, KBUILD_MODNAME);

__visible struct module __this_module
__section(".gnu.linkonce.this_module") = {
	.name = KBUILD_MODNAME,
	.init = init_module,
#ifdef CONFIG_MODULE_UNLOAD
	.exit = cleanup_module,
#endif
	.arch = MODULE_ARCH_INIT,
};

#ifdef CONFIG_RETPOLINE
MODULE_INFO(retpoline, "Y");
#endif

static const struct modversion_info ____versions[]
__used __section("__versions") = {
	{ 0x2c635209, "module_layout" },
	{ 0xc6c777de, "put_devmap_managed_page" },
	{ 0x53b954a2, "up_read" },
	{ 0x54b1fac6, "__ubsan_handle_load_invalid_value" },
	{ 0x41ed3709, "get_random_bytes" },
	{ 0xc7a4fbed, "rtnl_lock" },
	{ 0xd89483c5, "netif_carrier_on" },
	{ 0x345bca34, "netif_carrier_off" },
	{ 0x56470118, "__warn_printk" },
	{ 0x837b7b09, "__dynamic_pr_debug" },
	{ 0x2d5f69b3, "rcu_read_unlock_strict" },
	{ 0x3213f038, "mutex_unlock" },
	{ 0x97651e6c, "vmemmap_base" },
	{ 0x7a5772ae, "__put_net" },
	{ 0x851c15cf, "kthread_create_on_node" },
	{ 0x15ba50a6, "jiffies" },
	{ 0x668b19a1, "down_read" },
	{ 0xe2d5255a, "strcmp" },
	{ 0x3363cbcd, "kthread_bind" },
	{ 0xbcd5486f, "__netdev_alloc_skb" },
	{ 0xd9a5ea54, "__init_waitqueue_head" },
	{ 0x5b8239ca, "__x86_return_thunk" },
	{ 0x38ccd1b1, "param_ops_charp" },
	{ 0xc2aad509, "misc_register" },
	{ 0xfb578fc5, "memset" },
	{ 0x6dcd3d64, "netif_rx_ni" },
	{ 0x4915337b, "unregister_pernet_subsys" },
	{ 0x6a2bfc93, "netif_tx_wake_queue" },
	{ 0x4c9f47a5, "current_task" },
	{ 0xcefb0c9f, "__mutex_init" },
	{ 0x488daa9c, "ethtool_op_get_link" },
	{ 0xa0aa95d4, "kthread_stop" },
	{ 0x5a5a2271, "__cpu_online_mask" },
	{ 0x9c1b39b3, "free_netdev" },
	{ 0x9166fada, "strncpy" },
	{ 0x73a5ad3b, "register_netdev" },
	{ 0x5a921311, "strncmp" },
	{ 0xa34547b4, "skb_push" },
	{ 0x4dfa8d4b, "mutex_lock" },
	{ 0xce807a25, "up_write" },
	{ 0x57bc19d2, "down_write" },
	{ 0xfe487975, "init_wait_entry" },
	{ 0x800473f, "__cond_resched" },
	{ 0x7cd8d75e, "page_offset_base" },
	{ 0x87a21cb3, "__ubsan_handle_out_of_bounds" },
	{ 0xa916b694, "strnlen" },
	{ 0x296695f, "refcount_warn_saturate" },
	{ 0xd0da656b, "__stack_chk_fail" },
	{ 0x882876e6, "eth_header_parse" },
	{ 0x8ddd8aad, "schedule_timeout" },
	{ 0x92997ed8, "_printk" },
	{ 0xe7fffe41, "alloc_netdev_mqs" },
	{ 0x65487097, "__x86_indirect_thunk_rax" },
	{ 0x58ca4ff9, "eth_type_trans" },
	{ 0x7f24de73, "jiffies_to_usecs" },
	{ 0x9ce84760, "wake_up_process" },
	{ 0x3de5fda1, "get_user_pages_remote" },
	{ 0xf48f66b2, "register_pernet_subsys" },
	{ 0xbdfb6dbb, "__fentry__" },
	{ 0xcbd4898c, "fortify_panic" },
	{ 0x99305ab, "ether_setup" },
	{ 0x3eeb2322, "__wake_up" },
	{ 0xb3f7646e, "kthread_should_stop" },
	{ 0x8c26d495, "prepare_to_wait_event" },
	{ 0x54496b4, "schedule_timeout_interruptible" },
	{ 0x69acdf38, "memcpy" },
	{ 0x6e9c1891, "dev_trans_start" },
	{ 0x92540fbf, "finish_wait" },
	{ 0x9e69cea5, "unregister_netdev" },
	{ 0xa487e741, "consume_skb" },
	{ 0x85670f1d, "rtnl_is_locked" },
	{ 0x28d384c6, "skb_put" },
	{ 0x13c49cc2, "_copy_from_user" },
	{ 0x4bc81f50, "misc_deregister" },
	{ 0x7b4da6ff, "__init_rwsem" },
	{ 0x6e720ff2, "rtnl_unlock" },
	{ 0xd54fe6d2, "__put_page" },
	{ 0x587f22d7, "devmap_managed_key" },
};

MODULE_INFO(depends, "");


MODULE_INFO(srcversion, "E4C0559CD946B5493246948");
