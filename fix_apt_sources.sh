#!/bin/bash
# 修复 apt CD-ROM 源问题

echo "修复 apt CD-ROM 源配置..."
echo "========================"

# 检查是否以 root 运行
if [ "$EUID" -ne 0 ]; then 
    echo "需要 root 权限"
    echo "运行: sudo $0"
    exit 1
fi

# 备份原始文件
echo "备份原始配置文件..."
cp /etc/apt/sources.list /etc/apt/sources.list.backup.$(date +%Y%m%d_%H%M%S) 2>/dev/null || true

# 注释掉 CD-ROM 源
if grep -q "file:///cdrom" /etc/apt/sources.list 2>/dev/null; then
    echo "找到 CD-ROM 源配置，正在注释..."
    sed -i 's|^deb.*file:///cdrom|# &|' /etc/apt/sources.list
    echo "已注释 CD-ROM 源"
else
    echo "未找到 CD-ROM 源配置"
fi

# 检查 sources.list.d 目录
if [ -d /etc/apt/sources.list.d ]; then
    for file in /etc/apt/sources.list.d/*.list; do
        if [ -f "$file" ] && grep -q "file:///cdrom" "$file" 2>/dev/null; then
            echo "在 $file 中找到 CD-ROM 源，正在注释..."
            sed -i 's|^deb.*file:///cdrom|# &|' "$file"
        fi
    done
fi

# 测试更新
echo ""
echo "测试 apt update..."
if apt update 2>&1 | grep -q "cdrom.*Release"; then
    echo "警告: 仍有 CD-ROM 相关错误"
    echo "尝试完全移除 CD-ROM 源..."
    sed -i '/cdrom/d' /etc/apt/sources.list 2>/dev/null || true
    apt update
else
    echo "apt update 成功！"
fi

echo ""
echo "完成！如果仍有问题，可以手动编辑 /etc/apt/sources.list"
