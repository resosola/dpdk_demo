#!/bin/bash
# 快速安装 DPDK 构建依赖

# 不立即退出，允许处理错误
set +e

echo "安装 DPDK 构建依赖..."
echo "===================="

# 检查是否以 root 运行
if [ "$EUID" -ne 0 ]; then 
    echo "需要 root 权限来安装依赖"
    echo "运行: sudo $0"
    exit 1
fi

# 更新包列表（处理 CD-ROM 源问题）
echo "更新包列表..."
# 检查并注释掉有问题的 CD-ROM 源
if grep -q "file:///cdrom" /etc/apt/sources.list 2>/dev/null; then
    echo "检测到 CD-ROM 源配置，暂时禁用以避免错误..."
    sed -i 's|^deb.*file:///cdrom|# &|' /etc/apt/sources.list
fi
# 更新包列表
apt update

# 安装基本构建工具
echo "安装基本构建工具..."
set -e  # 从这里开始，安装失败则退出
apt install -y \
    build-essential \
    meson \
    ninja-build \
    pkg-config \
    python3-pip \
    python3-setuptools \
    wget \
    tar

# 安装 Python 依赖（DPDK 编译需要）
echo "安装 Python 依赖..."
pip3 install --upgrade meson pyelftools

# 验证安装
echo ""
echo "验证安装..."
echo "=========="
meson --version || echo "警告: meson 未正确安装"
ninja --version || echo "警告: ninja 未正确安装"
pkg-config --version || echo "警告: pkg-config 未正确安装"

echo ""
echo "依赖安装完成！"
echo ""
echo "下一步："
echo "  1. 下载 DPDK: wget https://fast.dpdk.org/rel/dpdk-21.11.tar.xz"
echo "  2. 运行安装脚本: ./install_dpdk.sh"
echo "  3. 编译应用: make"
