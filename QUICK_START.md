# 快速开始指南

## 一、环境准备

### 1. 安装构建依赖

```bash
# 安装必要的构建工具
sudo apt update
sudo apt install -y meson ninja pkg-config python3-pip
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

# 加载模块
sudo modprobe rte_kni

# 验证
lsmod | grep kni
```

**注意**: 如果遇到 "Module rte_kni not found" 错误，说明 KNI 内核模块尚未编译，请按照上述步骤编译并安装模块。

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
sudo ip link set ens34 down

# 2. 删除 IP 地址（如果有多个，需要逐个删除）
sudo ip addr flush dev ens34

# 3. 绑定网卡（例如：ens34）
sudo dpdk-devbind.py --bind=uio_pci_generic ens34

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
sudo ./dpdk_filter -l 0-3 -n 4 -- -i 0 -f tun0 -d 192.168.182.128:7777
```

### 示例 2: 转发到物理网卡并使用 KNI

```bash
# 1. 运行程序（转发匹配规则的数据包到 ens35，其他数据包通过 KNI 注入内核）
sudo ./dpdk_filter -l 0-3 -n 4 -- -i 0 -f ens34 -s 192.168.182.1:8752 -k vEth0

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

### 测试转发功能

```bash
# 在另一个终端发送测试数据包
# 使用 nc 或 ping 测试匹配规则的数据包是否被转发
nc -zv 192.168.182.128 7777
```

### 测试 KNI 功能

```bash
# 发送不匹配规则的数据包，应该通过 KNI 进入内核协议栈
ping 8.8.8.8
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
