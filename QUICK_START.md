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

# 编译 DPDK
meson build
cd build
ninja

# 可选：安装到系统
sudo ninja install
sudo ldconfig
```

**注意**: 如果遇到 "meson 命令未找到" 错误，请参考 `BUILD.md` 文件中的详细说明。

### 2. 配置大页内存

```bash
# 分配 1GB 大页内存（2048 个 2MB 页面）
echo 1024 > /sys/kernel/mm/hugepages/hugepages-2048kB/nr_hugepages

# 验证
cat /proc/meminfo | grep Huge
```

### 3. 加载 KNI 内核模块

```bash
# 加载 DPDK KNI 模块
sudo modprobe rte_kni

# 验证
lsmod | grep kni
```

### 4. 绑定网卡到 DPDK

```bash
# 查看网卡状态
sudo dpdk-devbind.py --status

# 绑定网卡（例如：ens35）
# 方法 1: 使用 igb_uio
sudo modprobe igb_uio
sudo dpdk-devbind.py --bind=igb_uio ens35

# 方法 2: 使用 vfio-pci（推荐，需要 IOMMU 支持）
sudo modprobe vfio-pci
sudo dpdk-devbind.py --bind=vfio-pci ens35

# 验证绑定
sudo dpdk-devbind.py --status
```

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
sudo ./dpdk_filter -l 0-3 -n 4 -- -i 0 -f ens35 -s 192.168.182.1:8752 -k vEth0

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
