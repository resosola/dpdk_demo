# 快速修复指南

## 问题 1: CD-ROM 源错误

**错误信息**: `E: The repository 'file:/cdrom focal Release' no longer has a Release file.`

**解决方法**:
```bash
sudo ./fix_apt_sources.sh
# 或者手动修复
sudo sed -i 's|^deb.*file:///cdrom|# &|' /etc/apt/sources.list
sudo apt update
```

## 问题 2: 缺少 Python 模块 elftools

**错误信息**: `ERROR: Problem encountered: missing python module: elftools`

**解决方法**:
```bash
sudo pip3 install pyelftools
```

## 问题 3: meson 命令未找到

**解决方法**:
```bash
sudo apt install meson ninja-build
# 或者使用 pip
sudo pip3 install meson
```

## 完整安装流程

```bash
# 1. 修复 apt 源（如果需要）
sudo ./fix_apt_sources.sh

# 2. 安装所有依赖
sudo ./install_deps.sh

# 3. 如果 install_deps.sh 没有安装 pyelftools，手动安装
sudo pip3 install pyelftools

# 4. 编译 DPDK
./install_dpdk.sh

# 5. 编译应用程序
make
```

## 验证依赖

运行以下命令检查所有依赖是否已安装：

```bash
# 检查系统工具
meson --version
ninja --version
pkg-config --version

# 检查 Python 模块
python3 -c "import elftools; print('pyelftools OK')"
python3 -c "import mesonbuild; print('meson OK')"
```
