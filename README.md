# DPDK 数据包过滤和转发 Demo

这是一个使用 DPDK 实现的数据包过滤和转发程序，功能类似于 `ebpf_tc_demo`，但使用 DPDK 进行高性能数据包处理。

## 功能特性

1. **高性能数据包处理**：使用 DPDK 绕过内核网络栈，实现零拷贝数据包处理
2. **灵活的过滤规则**：支持基于源/目的 IP 和端口的过滤规则
3. **数据包转发**：匹配规则的数据包可以转发到 TUN 设备或物理网卡
4. **内核协议栈集成**：未转发的数据包通过 KNI (Kernel NIC Interface) 注入回内核协议栈，保证正常网络功能

## 工作原理

```
数据包到达网卡
    ↓
DPDK 接收数据包
    ↓
解析 IPv4 TCP/UDP 数据包
    ↓
匹配过滤规则？
    ↓
是 → 转发到目标接口 (TUN 或物理网卡)
否 → 通过 KNI 注入回内核协议栈
```

## 依赖要求

- DPDK (Data Plane Development Kit) 已安装
- Linux 内核支持 KNI (通常需要加载 `rte_kni` 内核模块)
- 大页内存已配置
- 需要 root 权限运行

## 编译

### 设置 DPDK 环境变量

```bash
export RTE_SDK=/path/to/dpdk
export RTE_TARGET=x86_64-native-linux-gcc
```

### 编译程序

```bash
make
```

## 使用方式

### 基本用法

```bash
# 使用 DPDK 处理端口 0，转发匹配的数据包到 tun0
sudo ./dpdk_filter -l 0-3 -n 4 -- -i 0 -f tun0 -d 192.168.182.128:7777

# 使用 KNI 将未转发的数据包注入回内核协议栈
sudo ./dpdk_filter -l 0-3 -n 4 -- -i 0 -f ens35 -s 192.168.182.1:8752 -k vEth0
```

### 参数说明

#### DPDK EAL 参数（在 `--` 之前）

- `-l <cores>`: 指定使用的 CPU 核心（例如：`-l 0-3`）
- `-n <channels>`: 指定内存通道数（例如：`-n 4`）
- `-m <memory>`: 指定内存大小（例如：`-m 1024`）

#### 应用参数（在 `--` 之后）

- `-i, --interface <port_id>`: DPDK 端口 ID（必需）
- `-f, --forward <ifname>`: 转发目标接口名称（必需）
- `-d, --dst <ip:port>`: 过滤目标 IP:端口规则（可多次指定）
- `-s, --src <ip:port>`: 过滤源 IP:端口规则（可多次指定）
- `-k, --kni <ifname>`: KNI 接口名称（可选，用于将数据包注入回内核）
- `-h, --help`: 显示帮助信息

### 完整示例

#### 1. 设置大页内存

```bash
# 分配 1GB 大页内存
echo 1024 > /sys/kernel/mm/hugepages/hugepages-2048kB/nr_hugepages
```

#### 2. 加载 KNI 内核模块（如果使用 KNI）

```bash
# 加载 DPDK KNI 内核模块
sudo modprobe rte_kni
```

#### 3. 绑定网卡到 DPDK

```bash
# 查看网卡状态
sudo dpdk-devbind.py --status

# 绑定网卡到 DPDK（例如：ens35）
sudo dpdk-devbind.py --bind=igb_uio ens35
# 或者使用 vfio-pci
sudo dpdk-devbind.py --bind=vfio-pci ens35
```

#### 4. 创建 TUN 设备（如果转发到 TUN）

```bash
sudo ip tuntap add mode tun name tun0
sudo ip addr add 10.0.0.1/24 dev tun0
sudo ip link set tun0 up
```

#### 5. 运行程序

```bash
# 示例 1: 转发匹配规则的数据包到 TUN 设备
sudo ./dpdk_filter -l 0-3 -n 4 -- -i 0 -f tun0 -d 192.168.182.128:7777

# 示例 2: 转发匹配规则的数据包到物理网卡，并使用 KNI 处理其他数据包
sudo ./dpdk_filter -l 0-3 -n 4 -- -i 0 -f ens35 -s 192.168.182.1:8752 -k vEth0
```

#### 6. 配置 KNI 接口（如果使用 KNI）

```bash
# 配置 KNI 接口的 IP 地址
sudo ip addr add 192.168.1.100/24 dev vEth0
sudo ip link set vEth0 up

# 添加路由（如果需要）
sudo ip route add default via 192.168.1.1 dev vEth0
```

## 与 eBPF 版本的对比

| 特性 | eBPF 版本 | DPDK 版本 |
|------|-----------|-----------|
| 性能 | 高（内核态处理） | 极高（用户态零拷贝） |
| 内核依赖 | 需要 eBPF 支持 | 需要 DPDK 和 KNI 支持 |
| 配置复杂度 | 低 | 中等（需要绑定网卡等） |
| 灵活性 | 高（动态加载） | 中等（需要重新编译） |
| 适用场景 | 通用网络过滤 | 高性能网络处理 |

## 注意事项

1. **网卡绑定**：DPDK 会接管网卡，网卡将不再出现在 `ifconfig` 或 `ip link` 中
2. **大页内存**：必须配置足够的大页内存，否则程序无法启动
3. **CPU 亲和性**：建议将程序绑定到特定 CPU 核心以获得最佳性能
4. **KNI 模块**：如果使用 KNI，需要加载 `rte_kni` 内核模块
5. **权限**：程序需要 root 权限运行

## 故障排查

### 问题：程序无法启动，提示内存不足

**解决方案**：增加大页内存配置
```bash
echo 2048 > /sys/kernel/mm/hugepages/hugepages-2048kB/nr_hugepages
```

### 问题：KNI 接口无法创建

**解决方案**：确保已加载 KNI 内核模块
```bash
sudo modprobe rte_kni
lsmod | grep kni
```

### 问题：网卡无法绑定到 DPDK

**解决方案**：
1. 检查网卡驱动是否支持 DPDK
2. 确保网卡未被其他程序使用
3. 尝试使用不同的 DPDK 驱动（`igb_uio` 或 `vfio-pci`）

### 问题：数据包无法转发

**解决方案**：
1. 检查目标接口是否存在且已启动
2. 检查转发接口的权限和配置
3. 查看程序输出的错误信息

## 许可证

Apache License 2.0
# dpdk_demo
