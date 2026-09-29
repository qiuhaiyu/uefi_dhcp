qemu-auto-dhcp


文件是DhcpTest. 是从 QEMU 自带的 DHCP 服务获取的 ip信息
```
Status = Dhcp4->Start(
        Dhcp4,
        NULL
    );
```
DhcpTest.efi
     │
     │ EFI_DHCP4_PROTOCOL
     ↓
Dhcp4Dxe
     ↓
Udp4Dxe / Ip4Dxe
     ↓
MnpDxe / SNP
     ↓
QEMU E1000 虚拟网卡
     │
     │
     ↓
-netdev user,id=net0
     │
     ├── QEMU 内置 DHCP
     ├── QEMU NAT
     └── 虚拟网络 10.0.2.0/24
