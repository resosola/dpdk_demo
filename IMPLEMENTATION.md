# 实现说明

## 概述

本实现使用 DPDK (Data Plane Development Kit) 重新实现了 `ebpf_tc_demo` 的功能，提供了高性能的数据包过滤和转发能力，同时保证未转发的流量可以正常流入网络协议栈。

## 核心功能

### 1. 数据包接收和处理

- 使用 DPDK 绕过内核网络栈，实现零拷贝数据包接收
- 支持多核处理，可以充分利用多核 CPU
- 使用 DPDK 的内存池管理，提高内存分配效率

### 2. 数据包解析

- 解析以太网头部，识别 IPv4 数据包
- 解析 IP 头部，提取源/目的 IP 地址
- 解析 TCP/UDP 头部，提取源/目的端口
- 只处理 TCP 和 UDP 协议的数据包

### 3. 过滤规则匹配

- 支持基于目标 IP+端口 的过滤规则（rule_type=0）
- 支持基于源 IP+端口 的过滤规则（rule_type=1）
- 支持多个过滤规则同时生效
- 使用链表存储过滤规则，支持动态添加

### 4. 数据包转发

匹配过滤规则的数据包会被转发到指定的目标接口：

- **转发到 TUN 设备**：如果目标接口是 TUN 设备（名称以 "tun" 开头），则：
  - 跳过以太网头部，提取 IP 数据包
  - 通过 `write()` 系统调用写入 TUN 设备文件描述符
  - TUN 设备会将数据包注入到内核网络栈

- **转发到物理网卡**：如果目标接口是物理网卡，则：
  - 保持完整的以太网帧
  - 通过 raw socket (`AF_PACKET`) 发送到目标网卡

### 5. 内核协议栈集成（KNI）

未匹配过滤规则的数据包通过 KNI (Kernel NIC Interface) 注入回内核协议栈：

- **KNI 工作原理**：
  - DPDK 创建一个虚拟网络接口（KNI 接口）
  - 未转发的数据包通过 `rte_kni_tx_burst()` 发送到 KNI
  - KNI 内核模块将数据包注入到内核网络栈
  - 内核网络栈正常处理这些数据包

- **双向通信**：
  - 从内核到 DPDK：内核发送的数据包通过 KNI 到达 DPDK
  - 从 DPDK 到内核：DPDK 通过 KNI 将数据包注入内核

## 与 eBPF 版本的对比

| 特性 | eBPF 版本 | DPDK 版本 |
|------|-----------|-----------|
| **处理位置** | 内核态 | 用户态 |
| **性能** | 高（内核态处理，减少上下文切换） | 极高（零拷贝，绕过内核） |
| **内存管理** | 内核管理 | DPDK 内存池（大页内存） |
| **CPU 使用** | 内核调度 | 用户态轮询（可绑定 CPU 核心） |
| **数据包路径** | 内核网络栈 → eBPF → 用户态 | DPDK → 用户态处理 → 转发/KNI |
| **未转发流量** | 继续在网络栈中处理 | 通过 KNI 注入回网络栈 |
| **配置复杂度** | 低（只需加载 eBPF 程序） | 中等（需要绑定网卡、配置大页内存） |
| **灵活性** | 高（动态加载 eBPF 程序） | 中等（需要重新编译） |
| **适用场景** | 通用网络过滤和监控 | 高性能网络处理（如网关、负载均衡） |

## 数据流图

### eBPF 版本数据流

```
网卡 → 内核网络栈 → TC Ingress Hook (eBPF)
                    ↓
           匹配规则？
            ↓              ↓
          是              否
           ↓              ↓
     perf event → 用户态转发    继续在网络栈中处理
```

### DPDK 版本数据流

```
网卡 → DPDK (用户态)
       ↓
   解析数据包
       ↓
   匹配规则？
       ↓              ↓
     是              否
       ↓              ↓
   转发到目标接口    KNI → 内核网络栈
```

## 关键实现细节

### 1. 多核处理

```c
// 在每个 CPU 核心上运行处理函数
rte_eal_mp_remote_launch(lcore_main, NULL, CALL_MASTER);
lcore_main(NULL);  // 主核心也运行
```

### 2. 数据包处理循环

```c
while (!force_quit) {
    // 处理 KNI 请求（从内核接收数据包）
    if (kni) {
        kni_request_handler(kni, NULL);
    }
    
    // 从网卡接收数据包
    nb_rx = rte_eth_rx_burst(port_id, 0, bufs, BURST_SIZE);
    
    // 处理每个数据包
    for (i = 0; i < nb_rx; i++) {
        // 解析、匹配、转发或注入 KNI
    }
}
```

### 3. KNI 初始化

```c
struct rte_kni_conf conf;
conf.name = kni_name;
conf.force_bind = 1;
conf.group_id = port_id;
kni = rte_kni_alloc(mbuf_pool, &conf, &ops);
```

### 4. 转发到 TUN 设备

```c
// 提取 IP 数据包（跳过以太网头部）
struct rte_ipv4_hdr *ipv4_hdr = rte_pktmbuf_mtod_offset(m, ...);
uint16_t ip_len = rte_be_to_cpu_16(ipv4_hdr->total_length);
write(tun_fd, ipv4_hdr, ip_len);  // 写入 TUN 设备
```

### 5. 转发到物理网卡

```c
// 发送完整的以太网帧
void *pkt_data = rte_pktmbuf_mtod(m, void *);
uint16_t pkt_len = rte_pktmbuf_pkt_len(m);
sendto(raw_sock, pkt_data, pkt_len, 0, ...);  // 通过 raw socket 发送
```

## 性能优化建议

1. **CPU 绑定**：使用 `taskset` 或 `cpuset` 将进程绑定到特定 CPU 核心
2. **大页内存**：确保分配足够的大页内存（推荐 1GB+）
3. **NUMA 感知**：在 NUMA 系统上，确保内存和 CPU 在同一 NUMA 节点
4. **中断处理**：对于高吞吐量场景，考虑禁用中断，使用轮询模式
5. **批处理**：使用 DPDK 的批处理 API 提高处理效率

## 限制和注意事项

1. **网卡绑定**：DPDK 会完全接管网卡，网卡将不再出现在 `ifconfig` 中
2. **内核模块**：需要加载 DPDK 相关的内核模块（如 `igb_uio` 或 `vfio-pci`）
3. **大页内存**：必须配置大页内存，否则程序无法启动
4. **权限**：需要 root 权限运行
5. **KNI 限制**：KNI 的性能可能不如直接转发，但对于保证网络功能是必要的

## 未来改进方向

1. **规则管理**：支持运行时动态添加/删除规则（通过共享内存或控制平面）
2. **统计信息**：添加更详细的统计信息（如每个规则的匹配次数）
3. **多端口支持**：支持同时处理多个网卡端口
4. **配置文件**：支持从配置文件读取规则和配置
5. **日志系统**：添加更完善的日志系统，便于调试和监控
