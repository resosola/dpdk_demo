# 快速开始指南

## 一、环境准备

### 1. 安装构建依赖

```bash
# 安装必要的构建工具
sudo apt update
sudo apt install -y meson ninja-build pkg-config python3-pip
sudo pip3 install meson
```

### 2. 安装 DPDK

#### 方法 A: 使用安装脚本（推荐）

```bash
cd /home/liu/projects/dpdk_demo

# 如果还没有 DPDK 源码，先下载
wget https://fast.dpdk.org/rel/dpdk-21.11.tar.xz

# 运行安装脚本
./install_dpdk.sh
```

#### 方法 B: 手动编译

```bash
# 下载 DPDK（如果还没有）
wget https://fast.dpdk.org/rel/dpdk-21.11.tar.xz
tar xf dpdk-21.11.tar.xz
cd dpdk-21.11

# 编译 DPDK（启用内核模块编译）
meson build -Denable_kmods=true
cd build
ninja

# 可选：安装到系统（包括内核模块）
sudo ninja install
sudo ldconfig
```

**注意**: 如果编译时遇到权限问题，可能需要使用 `sudo` 运行 `meson` 和 `ninja` 命令。

**注意**: 如果遇到 "meson 命令未找到" 错误，请参考 `BUILD.md` 文件中的详细说明。

### 2. 配置大页内存

```bash
# 分配 1GB 大页内存（2048 个 2MB 页面）
echo 1024 > /sys/kernel/mm/hugepages/hugepages-2048kB/nr_hugepages

# 验证
cat /proc/meminfo | grep Huge
```

### 3. 编译和加载 KNI 内核模块

#### 如果 DPDK 编译时启用了内核模块（-Denable_kmods=true）

```bash
# 加载 DPDK KNI 模块
sudo modprobe rte_kni

# 验证
lsmod | grep kni
```

#### 如果模块不存在，需要手动编译 KNI 模块

```bash
# 确保已安装内核头文件
sudo apt install -y linux-headers-$(uname -r)

# 进入 DPDK 源码目录
cd /home/liu/dpdk_demo/dpdk-21.11

# 设置变量
KERNEL_DIR=/lib/modules/$(uname -r)/build
DPDK_ROOT=$(pwd)

# 编译 KNI 内核模块
cd kernel/linux/kni
sudo make -C $KERNEL_DIR M=$(pwd) src=$(pwd) \
    MODULE_CFLAGS="-include $DPDK_ROOT/config/rte_config.h \
    -I$DPDK_ROOT/lib/eal/include \
    -I$DPDK_ROOT/lib/kni \
    -I$DPDK_ROOT/build \
    -I$(pwd)" \
    modules

# 安装模块
sudo make -C $KERNEL_DIR M=$(pwd) modules_install

# 更新模块依赖数据库（重要！）
sudo depmod -a

# 加载模块
sudo modprobe rte_kni

# 验证
lsmod | grep kni
```

**注意**: 
- 如果遇到 "Module rte_kni not found" 错误：
  1. 确保已运行 `sudo depmod -a` 更新模块依赖数据库
  2. 如果 `modprobe` 仍然失败，可以尝试直接加载：`sudo insmod /lib/modules/$(uname -r)/extra/rte_kni.ko`
  3. 验证模块文件是否存在：`ls -la /lib/modules/$(uname -r)/extra/rte_kni.ko`
- 如果安装时出现 "missing 'System.map' file" 警告，这是正常的，不影响模块使用

### 4. 绑定网卡到 DPDK

#### 选择驱动模块

DPDK 支持三种驱动模块来绑定网卡：

1. **uio_pci_generic**（最简单，推荐新手使用）
   - Linux 内核自带，无需编译
   - 功能有限，不支持虚拟功能（VF）

2. **vfio-pci**（推荐，性能最好）
   - Linux 内核自带（builtin）
   - 需要 IOMMU 支持
   - 支持 SR-IOV 和虚拟功能

3. **igb_uio**（需要单独编译）
   - 需要从 dpdk-kmods 仓库编译
   - 功能最全面，但需要额外步骤

#### 方法 1: 使用 uio_pci_generic（最简单）

```bash
# 查看网卡状态
sudo dpdk-devbind.py --status

# 加载 uio_pci_generic 模块（Linux 内核自带）
sudo modprobe uio_pci_generic

# 如果网卡正在使用（有 IP 地址和路由），需要先关闭网卡
# 1. 关闭网卡
sudo ip link set enp5s0 down

# 2. 删除 IP 地址（如果有多个，需要逐个删除）
sudo ip addr flush dev enp5s0

# 3. 绑定网卡（例如：ens34）
sudo dpdk-devbind.py --bind=uio_pci_generic enp5s0

# 4. 验证绑定
sudo dpdk-devbind.py --status
```

**注意**: 如果遇到 "Warning: routing table indicates that interface is active" 错误：
- 说明网卡正在被内核使用（有 IP 地址配置或路由）
- 需要先关闭网卡并删除 IP 地址（见上述步骤 1-2）
- 或者使用 `--force` 参数强制绑定（会断开网络连接，不推荐）

#### 方法 2: 使用 vfio-pci（推荐，需要 IOMMU 支持）

```bash
# 查看网卡状态
sudo dpdk-devbind.py --status

# 检查 IOMMU 是否启用
dmesg | grep -i iommu
# 如果看到 "DMAR: IOMMU enabled" 或类似信息，说明 IOMMU 已启用

# 加载 vfio 相关模块
sudo modprobe vfio
sudo modprobe vfio-pci

# 如果没有 IOMMU，可以使用 no-IOMMU 模式（不安全，仅用于测试）
# sudo modprobe vfio enable_unsafe_noiommu_mode=1
# sudo modprobe vfio-pci

# 如果网卡正在使用，需要先关闭网卡
sudo ip link set ens34 down
sudo ip addr flush dev ens34

# 绑定网卡（例如：ens34）
sudo dpdk-devbind.py --bind=vfio-pci ens34

# 验证绑定
sudo dpdk-devbind.py --status
```

#### 方法 3: 使用 igb_uio（需要编译）

如果前两种方法不满足需求，可以使用 `igb_uio`：

```bash
# 1. 克隆 dpdk-kmods 仓库
cd /tmp
git clone http://dpdk.org/git/dpdk-kmods
cd dpdk-kmods/linux/igb_uio

# 2. 编译 igb_uio 模块
make

# 3. 加载 uio 基础模块
sudo modprobe uio

# 4. 加载 igb_uio 模块
sudo insmod igb_uio.ko

# 5. 验证模块已加载
lsmod | grep igb_uio

# 6. 绑定网卡
cd /home/liu/dpdk_demo
sudo dpdk-devbind.py --bind=igb_uio ens34

# 7. 验证绑定
sudo dpdk-devbind.py --status
```

**注意**: 
- `igb_uio` 模块在 DPDK 21.11 中已从主仓库移除，需要从 `dpdk-kmods` 单独编译
- 如果遇到 "Module igb_uio not found" 错误，说明模块尚未编译，请按照方法 3 编译
- 推荐优先尝试方法 1（uio_pci_generic）或方法 2（vfio-pci），它们更简单且无需编译

## 二、编译程序

```bash
cd /home/liu/projects/dpdk_demo

# 如果使用本地编译的 DPDK（未安装到系统），设置环境变量：
# export RTE_SDK=$(pwd)/dpdk-21.11/build

# 编译应用程序
make
```

如果编译失败，请检查：
1. DPDK 是否已正确安装或编译
2. 环境变量是否正确设置
3. 参考 `BUILD.md` 获取更多帮助

## 三、使用示例

### 示例 1: 转发到 TUN 设备

```bash
# 1. 创建 TUN 设备
sudo ip tuntap add mode tun name tun0
sudo ip addr add 10.0.0.1/24 dev tun0
sudo ip link set tun0 up

# 2. 运行程序（转发匹配规则的数据包到 tun0）
sudo ./dpdk_filter -l 0-3 -n 4 -- -i 0 -f tun0 -d 192.168.209.89:7777 -k vEth0
```

**命令说明**：
- `-l 0-3`: 使用 CPU 核心 0-3 处理数据包
- `-n 4`: 内存通道数为 4
- `-i 0`: 从 DPDK 端口 0（对应绑定的网卡）接收数据包
- `-f tun0`: 匹配的数据包转发到 `tun0` 设备
- `-d 192.168.182.128:7777`: 过滤规则，匹配**目的 IP** 为 `192.168.182.128` 且**目的端口**为 `7777` 的数据包

**工作流程**：
1. 从端口 0 绑定的网卡接收数据包
2. 检查是否为 IPv4 的 TCP/UDP 包（其他协议会被丢弃或发送到 KNI）
3. 提取数据包的目的 IP 和目的端口
4. 如果匹配规则（目的 IP = 192.168.182.128 且目的端口 = 7777）：
   - ✅ 转发到 `tun0` 设备（去掉以太网头，只写入 IP 层数据）
5. 如果不匹配：
   - 如果配置了 KNI（`-k` 参数），发送到 KNI 接口注入内核协议栈
   - 否则直接丢弃

**注意**：
- `-d` 参数匹配的是**目的 IP 和目的端口**（destination）
- `-s` 参数匹配的是**源 IP 和源端口**（source）
- 可以同时指定多个 `-d` 或 `-s` 规则，任意一个匹配即转发
- 只处理 IPv4 的 TCP/UDP 数据包

### 示例 2: 转发到物理网卡并使用 KNI

```bash
# 1. 运行程序（转发匹配规则的数据包到 ens35，其他数据包通过 KNI 注入内核）
sudo ./dpdk_filter -l 0-3 -n 4 -- -i 0 -f enp5s0 -s 192.168.209.89:8752 -k vEth0

# 2. 配置 KNI 接口（在另一个终端）
sudo ip addr add 192.168.1.100/24 dev vEth0
sudo ip link set vEth0 up
```

### 示例 3: 多个过滤规则

```bash
# 同时过滤多个目标端口和源端口
sudo ./dpdk_filter -l 0-3 -n 4 -- -i 0 -f tun0 \
    -d 192.168.182.128:7777 \
    -d 192.168.182.128:8080 \
    -s 192.168.182.1:8752 \
    -k vEth0
```

## 四、验证功能

### DPDK 绑定网卡后的外部访问方案说明

#### 当前方案的定位

**KNI (Kernel NIC Interface) 是 DPDK 官方提供的标准方案**，用于 DPDK 应用与内核协议栈之间的数据包交换。根据 DPDK 官方文档：

> "KNI allows userspace applications access to the Linux control plane"
> "Allows management of DPDK ports using standard Linux net tools"

**当前实现方案**：
- ✅ 使用 KNI 接口（`-k vEth0`）- **这是 DPDK 的标准方案**
- ✅ 使用 TUN 设备转发匹配的数据包 - 这是应用层实现
- ✅ 不匹配的数据包通过 KNI 注入内核协议栈 - **标准做法**

#### 标准生产环境方案对比

| 方案 | 适用场景 | 优点 | 缺点 | 标准性 |
|------|---------|------|------|--------|
| **KNI 接口** | 需要内核协议栈支持的管理/控制流量 | DPDK 官方标准，比 TUN/TAP 更快 | 需要内核模块支持 | ⭐⭐⭐⭐⭐ |
| **混合架构** | 生产环境（推荐） | 部分网卡 DPDK，部分网卡内核 | 需要多网卡 | ⭐⭐⭐⭐⭐ |
| **应用层协议栈** | 高性能服务 | DPDK 直接实现 TCP/UDP | 开发复杂度高 | ⭐⭐⭐⭐ |
| **虚拟化方案** | 云环境 | 通过虚拟网络访问 | 需要虚拟化支持 | ⭐⭐⭐⭐ |

#### 标准生产环境推荐方案

**方案 1: 混合架构（最推荐）**
```bash
# 网卡 1: 绑定到 DPDK（高性能数据平面）
sudo dpdk-devbind.py --bind=uio_pci_generic enp5s0

# 网卡 2: 保留给内核（管理平面，外部访问）
# 不绑定，正常使用内核驱动
sudo ip addr add 192.168.209.100/24 dev enp5s1
sudo ip link set enp5s1 up

# 外部通过网卡 2 访问服务器
ssh user@192.168.209.100
```

**方案 2: KNI 接口（当前方案，标准但有限制）**
```bash
# 使用 KNI 接口作为管理接口
sudo ./dpdk_filter -l 0-3 -n 4 -- -i 0 -f tun0 -d 192.168.209.89:7777 -k vEth0
sudo ip addr add 192.168.209.100/24 dev vEth0
sudo ip link set vEth0 up

# 外部通过 KNI 接口访问
ssh user@192.168.209.100
```

**方案 3: 应用层协议栈（高性能服务）**
```bash
# DPDK 应用直接实现 TCP/UDP 协议栈
# 例如：使用 DPDK 的 l2fwd、l3fwd 等示例
# 或使用 DPDK 的 TCP/IP 协议栈库
```

#### 当前方案的优缺点

**优点**：
- ✅ 使用 DPDK 官方标准 KNI 接口
- ✅ 比 TUN/TAP 性能更好（避免系统调用和用户空间拷贝）
- ✅ 可以使用标准 Linux 网络工具管理
- ✅ 适合开发测试和特定场景

**缺点**：
- ⚠️ 需要加载内核模块（rte_kni）
- ⚠️ 外部访问需要通过 KNI 接口，不是直接访问物理网卡
- ⚠️ 配置相对复杂
- ⚠️ 不适合所有生产场景

#### 总结

**当前方案是标准的 DPDK 方案**，但需要注意：
1. **KNI 是 DPDK 官方标准** - 用于 DPDK 与内核协议栈交互
2. **外部访问方式** - 通过 KNI 接口访问是标准做法，但不是唯一方案
3. **生产环境建议** - 推荐使用混合架构（部分网卡 DPDK，部分网卡内核）
4. **适用场景** - 当前方案适合开发测试、特定过滤场景，不适合需要直接访问物理网卡 IP 的场景

### 重要说明：网卡绑定后的访问方式

**当网卡绑定到 DPDK 后**：
- 网卡不再出现在 `ip link` 中，内核无法直接使用
- **外部访问需要通过 KNI 接口**（如果配置了 `-k` 参数）
- KNI 接口（如 `vEth0`）是 DPDK 和内核之间的桥梁

### 验证步骤（以示例 1 为例）

#### 步骤 1: 准备环境

```bash
# 1. 创建 TUN 设备
sudo ip tuntap add mode tun name tun0
sudo ip addr add 10.0.0.1/24 dev tun0
sudo ip link set tun0 up

# 2. 配置 KNI 接口（在程序启动后，另一个终端执行）
# 注意：KNI 接口会在程序启动时自动创建
sudo ip addr add 192.168.209.89/24 dev vEth0  # 使用与网卡相同的网段
sudo ip link set vEth0 up

# 3. 验证 KNI 接口
ip link show vEth0
ip addr show vEth0
```

#### 步骤 2: 启动程序

```bash
# 终端 1: 运行 DPDK 程序
sudo ./dpdk_filter -l 0-3 -n 4 -- -i 0 -f tun0 -d 192.168.209.89:7777 -k vEth0
```

**程序启动后应该看到**：
```
EAL: Detected CPU lcores: ...
EAL: Detected NUMA nodes: ...
...
Port 0 MAC: XX:XX:XX:XX:XX:XX
Opened TUN device tun0 for forwarding
Created KNI interface: vEth0
Added filter rule: forward packets to 192.168.209.89:7777
Core X processing packets.
```

#### 步骤 3: 验证转发功能（匹配规则的数据包）

**终端 2: 发送匹配规则的数据包**

```bash
# 方法 1: 使用 nc 发送 TCP 数据包到匹配的目的地址和端口
# 注意：需要从外部机器或通过其他网卡发送
nc -zv 192.168.209.89 7777

# 方法 2: 使用 curl 测试
curl -v http://192.168.209.89:7777

# 方法 3: 使用 hping3 发送 UDP 数据包
hping3 -2 -p 7777 192.168.209.89 -c 5
```

**验证转发是否成功**：
```bash
# 检查 TUN 设备是否收到数据包（在终端 3）
sudo tcpdump -i tun0 -n
# 如果看到数据包，说明转发成功
```

#### 步骤 4: 验证 KNI 功能（不匹配规则的数据包）

**终端 2: 通过 KNI 接口访问网络**

```bash
# 方法 1: 通过 KNI 接口 ping（不匹配规则，会进入内核协议栈）
ping -I vEth0 8.8.8.8

# 方法 2: 通过 KNI 接口访问其他服务
curl --interface vEth0 http://www.baidu.com

# 方法 3: 发送到不匹配的端口（会通过 KNI 注入内核）
nc -zv 192.168.209.89 8888  # 端口不匹配，会通过 KNI
```

**验证 KNI 是否工作**：
```bash
# 检查 KNI 接口的流量统计
ip -s link show vEth0

# 检查路由表（确保通过 vEth0 的路由正确）
ip route show dev vEth0
```

#### 步骤 5: 查看程序统计信息

**终端 1: 按 Ctrl+C 停止程序后，会显示统计信息**：
```
Statistics:
  Total packets: XXXX
  Forwarded packets: XXXX    # 匹配规则并转发到 tun0 的数据包数
  KNI packets (to kernel): XXXX  # 不匹配规则，通过 KNI 注入内核的数据包数
```

### 外部访问绑定的网卡

**场景：外部机器如何访问绑定到 DPDK 的网卡？**

#### 方法 1: 通过 KNI 接口访问（推荐）

1. **配置 KNI 接口的 IP 地址**（与原始网卡在同一网段）：
```bash
# 假设原始网卡 IP 是 192.168.209.100/24
sudo ip addr add 192.168.209.100/24 dev vEth0
sudo ip link set vEth0 up

# 添加默认路由（如果需要）
sudo ip route add default via 192.168.209.1 dev vEth0
```

2. **外部机器访问**：
   - 外部机器可以像访问普通网卡一样访问 KNI 接口的 IP
   - 数据包流程：外部 → 物理网卡（DPDK 接收）→ KNI 接口 → 内核协议栈 → 应用程序

3. **验证外部访问**：
```bash
# 在外部机器上
ping 192.168.209.100  # 应该能 ping 通 KNI 接口的 IP
ssh user@192.168.209.100  # 可以通过 SSH 访问
```

#### 方法 2: 通过 TUN 设备访问（仅匹配规则的数据包）

1. **配置 TUN 设备**：
```bash
sudo ip addr add 10.0.0.1/24 dev tun0
sudo ip link set tun0 up
```

2. **在 TUN 设备上运行服务**：
```bash
# 例如：在 TUN 设备上监听服务
nc -l 10.0.0.1 7777
```

3. **外部访问**：
   - 外部发送到 `192.168.209.89:7777` 的数据包会被转发到 `tun0`
   - 应用程序可以从 `tun0` 读取数据包

### 完整测试示例

```bash
# ===== 终端 1: 启动 DPDK 程序 =====
sudo ./dpdk_filter -l 0-3 -n 4 -- -i 0 -f tun0 -d 192.168.209.89:7777 -k vEth0

# ===== 终端 2: 配置 KNI 接口 =====
sudo ip addr add 192.168.209.100/24 dev vEth0
sudo ip link set vEth0 up
ip link show vEth0

# ===== 终端 3: 监听 TUN 设备（验证转发） =====
sudo tcpdump -i tun0 -n -v

# ===== 终端 4: 发送测试数据包 =====
# 测试 1: 匹配规则的数据包（会被转发到 tun0）
hping3 -2 -p 7777 192.168.209.89 -c 5

# 测试 2: 不匹配规则的数据包（会通过 KNI）
ping -I vEth0 8.8.8.8

# ===== 外部机器测试 =====
# 从外部机器访问 KNI 接口的 IP
ping 192.168.209.100
ssh user@192.168.209.100
```

### 常见问题排查

**Q: KNI 接口创建失败**
```bash
# 检查 KNI 模块是否加载
lsmod | grep kni

# 如果没有，加载模块
sudo modprobe rte_kni

# 检查模块是否正常
dmesg | grep kni
```

**Q: 外部无法访问 KNI 接口**
```bash
# 1. 检查 KNI 接口是否启动
ip link show vEth0

# 2. 检查 IP 地址配置
ip addr show vEth0

# 3. 检查路由表
ip route show

# 4. 检查防火墙规则
sudo iptables -L -n -v
```

**Q: 数据包没有被转发**
```bash
# 1. 检查程序是否正常运行（查看终端 1 的输出）

# 2. 检查过滤规则是否正确
# 确保目的 IP 和端口匹配

# 3. 检查 TUN 设备是否正常
ip link show tun0
sudo tcpdump -i tun0 -n

# 4. 检查数据包是否到达（使用 tcpdump 抓包）
sudo tcpdump -i any -n host 192.168.209.89
```

## 五、恢复网卡

```bash
# 停止程序（Ctrl+C）

# 解绑网卡
sudo dpdk-devbind.py --unbind ens35

# 绑定回内核驱动
sudo dpdk-devbind.py --bind=ixgbe ens35  # 根据实际驱动调整

# 验证
ip link show ens35
```

## 六、常见问题

### Q: 程序启动失败，提示 "No Ethernet ports - bye"

**A**: 这表示 DPDK 没有检测到任何绑定的网卡。请按以下步骤排查：

```bash
# 1. 检查网卡绑定状态
sudo dpdk-devbind.py --status

# 应该看到类似这样的输出：
# Network devices using DPDK-compatible driver
# ============================================
# 0000:02:02.0 '82545EM Gigabit Ethernet Controller' drv=uio_pci_generic unused=

# 如果没有看到任何 "drv=uio_pci_generic" 或 "drv=vfio-pci" 的网卡，说明网卡未绑定

# 2. 如果网卡未绑定，按照以下步骤绑定：
# a) 查看可用网卡
sudo dpdk-devbind.py --status

# b) 加载驱动模块
sudo modprobe uio_pci_generic
# 或
sudo modprobe vfio-pci

# c) 关闭网卡（如果正在使用）
sudo ip link set <网卡名> down
sudo ip addr flush dev <网卡名>

# d) 绑定网卡
sudo dpdk-devbind.py --bind=uio_pci_generic <网卡名或PCI地址>
# 例如：
sudo dpdk-devbind.py --bind=uio_pci_generic enp5s0

# 3. 验证绑定
sudo dpdk-devbind.py --status

# 4. 检查大页内存（虽然错误信息可能显示，但 2MB 大页内存通常足够）
cat /sys/kernel/mm/hugepages/hugepages-2048kB/nr_hugepages
# 如果为 0，需要配置：
echo 1024 > /sys/kernel/mm/hugepages/hugepages-2048kB/nr_hugepages

# 5. 重新运行程序
sudo ./dpdk_filter -l 0-3 -n 4 -- -i 0 -f tun0 -d 192.168.209.89:7777 -k vEth0
```

**常见原因**：
- 网卡没有绑定到 DPDK 驱动（最常见）
- 网卡正在被内核使用（有 IP 地址配置）
- 驱动模块未加载（uio_pci_generic 或 vfio-pci）

**验证网卡已正确绑定**：
```bash
sudo dpdk-devbind.py --status

# 应该看到类似输出：
# Network devices using DPDK-compatible driver
# ============================================
# 0000:05:00.0 'RTL8125 2.5GbE Controller' drv=uio_pci_generic unused=r8169,vfio-pci
```

**如果网卡已绑定但程序仍报错 "No Ethernet ports - bye"**：

可能原因：DPDK 21.11 可能不支持某些网卡（如 RTL8125），或者需要使用 `vfio-pci` 驱动。

**解决方案 1: 尝试使用 vfio-pci 驱动**（推荐）
```bash
# 1. 解绑当前驱动
sudo dpdk-devbind.py --unbind 0000:05:00.0

# 2. 加载 vfio-pci 模块（如果没有 IOMMU，使用 no-IOMMU 模式）
sudo modprobe vfio enable_unsafe_noiommu_mode=1
sudo modprobe vfio-pci

# 3. 绑定到 vfio-pci
sudo dpdk-devbind.py --bind=vfio-pci 0000:05:00.0

# 4. 验证绑定
sudo dpdk-devbind.py --status

# 5. 重新运行程序
sudo ./dpdk_filter -l 0-3 -n 4 -- -i 0 -f tun0 -d 192.168.209.89:7777 -k vEth0
```

**解决方案 2: 检查 DPDK 是否支持该网卡**
```bash
# 检查 DPDK 的 PMD 驱动列表
ls /usr/local/lib/x86_64-linux-gnu/ | grep -i pmd | grep -i net

# 如果找不到对应的 PMD 驱动，可能需要：
# 1. 使用更新的 DPDK 版本
# 2. 使用 vfio-pci 驱动（通用驱动，支持更多网卡）
# 3. 使用其他支持的网卡进行测试
```

如果网卡已绑定但程序仍报错，请检查：
1. **DPDK 是否正确安装**：
```bash
# 检查 DPDK 库路径
ldconfig -p | grep dpdk

# 检查程序是否能找到 DPDK 库
ldd ./dpdk_filter | grep dpdk
```

2. **大页内存是否挂载**：
```bash
# 检查大页内存挂载点
mount | grep hugepages
# 如果没有挂载，需要挂载：
sudo mkdir -p /dev/hugepages
sudo mount -t hugetlbfs nodev /dev/hugepages
```

3. **重新编译程序**（如果 DPDK 安装路径有变化）：
```bash
make clean
make
```

### Q: 绑定网卡时提示 "Warning: routing table indicates that interface is active"

**A**: 网卡正在被内核使用（有 IP 地址配置或路由），需要先关闭网卡：

```bash
# 1. 关闭网卡
sudo ip link set <网卡名> down

# 2. 删除所有 IP 地址
sudo ip addr flush dev <网卡名>

# 3. 使用 PCI 地址绑定（如果接口名无法识别）
sudo dpdk-devbind.py --bind=uio_pci_generic <PCI地址>
# 例如：sudo dpdk-devbind.py --bind=uio_pci_generic 0000:02:02.0
```

**注意**: 
- 绑定到 DPDK 后，网卡将不再出现在 `ip link` 中
- 如果这是唯一的网络连接，绑定后可能会断开网络
- 建议保留至少一个网卡不绑定到 DPDK，用于管理连接

### Q: 绑定失败，提示 "Cannot bind to driver: [Errno 19] No such device"

**A**: 这通常发生在以下情况：

1. **虚拟机环境限制**（如 VMware）：
   - VMware 虚拟网卡可能不完全支持 DPDK 绑定
   - 尝试重启虚拟机或使用物理机测试
   - 检查虚拟机设置中是否启用了 IOMMU/VT-d 支持

2. **设备状态异常**：
   ```bash
   # 检查设备是否真的存在
   lspci | grep <PCI地址>
   
   # 检查设备驱动绑定状态
   readlink /sys/bus/pci/devices/<PCI地址>/driver
   
   # 尝试恢复设备到内核驱动
   sudo dpdk-devbind.py --bind=e1000 <PCI地址>  # 根据实际驱动调整
   sudo ip link set <网卡名> up
   ```

3. **使用 --force 参数**（谨慎使用）：
   ```bash
   sudo dpdk-devbind.py --bind=uio_pci_generic --force <PCI地址>
   ```

4. **替代方案**：
   - 如果无法绑定，可以考虑使用 DPDK 的 `--vdev` 选项创建虚拟设备
   - 或者使用其他支持 DPDK 的网卡进行测试

### Q: 程序启动失败，提示 "Cannot init EAL"

**A**: 检查大页内存配置和 DPDK 环境变量

### Q: KNI 接口无法创建

**A**: 确保已加载 `rte_kni` 内核模块

### Q: 数据包无法转发

**A**: 检查目标接口是否存在且已启动，检查程序输出的错误信息

### Q: 性能不佳

**A**: 
- 确保程序运行在正确的 CPU 核心上
- 检查 CPU 频率调节器设置
- 使用 `taskset` 绑定进程到特定核心
