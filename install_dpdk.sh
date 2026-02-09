#!/bin/bash
# DPDK 安装脚本

set -e

DPDK_VERSION="21.11"
DPDK_DIR="dpdk-${DPDK_VERSION}"
DPDK_TAR="${DPDK_DIR}.tar.xz"
INSTALL_PREFIX="/usr/local"

echo "DPDK 安装脚本"
echo "=============="

# 检查是否已安装必要的工具
echo "检查依赖..."
MISSING_DEPS=()

if ! command -v meson &> /dev/null; then
    MISSING_DEPS+=("meson")
fi

if ! command -v ninja &> /dev/null; then
    MISSING_DEPS+=("ninja")
fi

if ! command -v pkg-config &> /dev/null; then
    MISSING_DEPS+=("pkg-config")
fi

# 检查 Python 模块
if ! python3 -c "import elftools" 2>/dev/null; then
    MISSING_PYTHON_DEPS=1
fi

if [ ${#MISSING_DEPS[@]} -ne 0 ] || [ -n "$MISSING_PYTHON_DEPS" ]; then
    echo "需要安装以下依赖:"
    if [ ${#MISSING_DEPS[@]} -ne 0 ]; then
        echo "  系统包: ${MISSING_DEPS[*]}"
    fi
    if [ -n "$MISSING_PYTHON_DEPS" ]; then
        echo "  Python 模块: pyelftools"
    fi
    echo ""
    echo "运行以下命令安装:"
    echo "  sudo ./install_deps.sh"
    echo "或者手动安装:"
    echo "  sudo apt install -y ${MISSING_DEPS[*]} python3-pip python3-setuptools"
    echo "  sudo pip3 install meson pyelftools"
    exit 1
fi

# 检查 DPDK 源码目录
if [ ! -d "$DPDK_DIR" ]; then
    if [ -f "$DPDK_TAR" ]; then
        echo "解压 DPDK 源码..."
        tar xf "$DPDK_TAR"
    else
        echo "错误: 找不到 DPDK 源码目录或压缩包"
        echo "请下载 DPDK ${DPDK_VERSION}:"
        echo "  wget https://fast.dpdk.org/rel/${DPDK_TAR}"
        exit 1
    fi
fi

cd "$DPDK_DIR"

# 配置构建目录
echo "配置 DPDK 构建..."
# 如果 build 目录存在但没有 build.ninja，需要重新配置
if [ -d "build" ] && [ ! -f "build/build.ninja" ]; then
    echo "检测到不完整的 build 目录，清理并重新配置..."
    rm -rf build
fi

if [ ! -d "build" ]; then
    echo "运行 meson 配置（启用内核模块编译）..."
    if ! meson setup build -Dprefix=$INSTALL_PREFIX -Denable_kmods=true; then
        echo "错误: meson 配置失败"
        echo "请检查错误信息，可能需要安装更多依赖"
        echo "提示: 如果编译内核模块失败，可能需要安装内核头文件:"
        echo "  sudo apt install -y linux-headers-$(uname -r)"
        exit 1
    fi
fi

# 编译
echo "编译 DPDK..."
cd build
if ! ninja; then
    echo "错误: ninja 编译失败"
    exit 1
fi

# 安装（可选）
read -p "是否安装 DPDK 到 $INSTALL_PREFIX? (需要 sudo) [y/N]: " -n 1 -r
echo
if [[ $REPLY =~ ^[Yy]$ ]]; then
    echo "安装 DPDK..."
    sudo ninja install
    sudo ldconfig
    echo "DPDK 已安装到 $INSTALL_PREFIX"
else
    echo "跳过安装。DPDK 已编译在 build 目录中。"
    echo "要使用本地编译的 DPDK，请设置环境变量:"
    echo "  export RTE_SDK=$(pwd)"
    echo "  export RTE_TARGET=x86_64-native-linux-gcc"
fi

echo ""
echo "完成！"
