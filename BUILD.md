# 编译指南

## 问题：meson 命令未找到

DPDK 21.11 使用 meson 和 ninja 作为构建系统。如果遇到 `meson` 命令未找到的错误，请按照以下步骤操作：

## 方法 1: 安装依赖并编译 DPDK（推荐）

### 1. 安装构建依赖

```bash
sudo apt update
sudo apt install -y meson ninja pkg-config python3-pip
sudo pip3 install meson
```

### 2. 使用安装脚本

```bash
cd /home/liu/projects/dpdk_demo
./install_dpdk.sh
```

脚本会自动：
- 检查依赖
- 解压 DPDK 源码（如果存在）
- 配置和编译 DPDK
- 可选安装到系统目录

### 3. 编译应用程序

```bash
make
```

## 方法 2: 手动编译 DPDK

### 1. 安装依赖

```bash
sudo apt install -y meson ninja pkg-config python3-pip
sudo pip3 install meson
```

### 2. 编译 DPDK

```bash
cd /home/liu/projects/dpdk_demo/dpdk-21.11
meson build
cd build
ninja
```

### 3. 设置环境变量并编译应用

```bash
cd /home/liu/projects/dpdk_demo
export RTE_SDK=$(pwd)/dpdk-21.11/build
make
```

## 方法 3: 使用系统包管理器安装 DPDK（如果可用）

某些 Linux 发行版提供预编译的 DPDK 包：

```bash
# Ubuntu/Debian (如果可用)
sudo apt install dpdk-dev

# 然后直接编译应用
make
```

## 验证安装

编译成功后，应该会生成 `dpdk_filter` 可执行文件：

```bash
ls -lh dpdk_filter
```

## 常见问题

### Q: meson 命令仍然找不到

**A**: 确保已安装并添加到 PATH：
```bash
which meson
# 如果找不到，尝试：
python3 -m mesonbuild.mesonmain --version
# 或者使用完整路径
```

### Q: ninja 命令找不到

**A**: 安装 ninja：
```bash
sudo apt install ninja-build
```

### Q: pkg-config 找不到 DPDK

**A**: 如果使用本地编译的 DPDK，需要设置环境变量：
```bash
export PKG_CONFIG_PATH=/path/to/dpdk/build/lib/x86_64-linux-gnu/pkgconfig:$PKG_CONFIG_PATH
```

### Q: 链接错误，找不到 DPDK 库

**A**: 确保 DPDK 库路径正确：
```bash
# 检查库文件是否存在
ls -la /usr/local/lib/x86_64-linux-gnu/librte_*.so
# 或者本地编译的路径
ls -la dpdk-21.11/build/lib/librte_*.so
```

### Q: 编译成功但运行时出错

**A**: 确保运行时库路径正确：
```bash
export LD_LIBRARY_PATH=/usr/local/lib/x86_64-linux-gnu:$LD_LIBRARY_PATH
# 或者添加到 /etc/ld.so.conf.d/dpdk.conf 并运行 sudo ldconfig
```

## 快速检查清单

- [ ] meson 已安装 (`meson --version`)
- [ ] ninja 已安装 (`ninja --version`)
- [ ] pkg-config 已安装 (`pkg-config --version`)
- [ ] DPDK 已编译或安装
- [ ] 环境变量已设置（如果使用本地编译）
- [ ] 大页内存已配置（运行程序时需要）
