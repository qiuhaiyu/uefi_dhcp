#include <Uefi.h>

#include <Library/UefiLib.h>
#include <Library/UefiApplicationEntryPoint.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/BaseMemoryLib.h>

#include <Protocol/ServiceBinding.h>
#include <Protocol/Dhcp4.h>


EFI_STATUS
EFIAPI
UefiMain(
    IN EFI_HANDLE        ImageHandle,
    IN EFI_SYSTEM_TABLE  *SystemTable
)
{
    EFI_STATUS                   Status;
    EFI_SERVICE_BINDING_PROTOCOL *Dhcp4Sb = NULL;
    EFI_DHCP4_PROTOCOL           *Dhcp4 = NULL;
    EFI_HANDLE                   DhcpChild = NULL;

    EFI_DHCP4_CONFIG_DATA        Config;
    EFI_DHCP4_MODE_DATA          Mode;

    (void)ImageHandle;
    (void)SystemTable;

    //
    // 先清零，避免后面 CLEANUP 时访问未初始化的指针
    //
    ZeroMem(&Config, sizeof(Config));
    ZeroMem(&Mode, sizeof(Mode));

    Print(L"\n");
    Print(L"============================================\n");
    Print(L"UEFI DHCP4 Protocol Test\n");
    Print(L"============================================\n");

    //
    // ========================================================
    // 1. 查找 DHCP4 Service Binding Protocol
    // ========================================================
    //
    Print(L"\n[1] Locate DHCP4 Service Binding...\n");

    Status = gBS->LocateProtocol(
        &gEfiDhcp4ServiceBindingProtocolGuid,
        NULL,
        (VOID **)&Dhcp4Sb
    );

    Print(
        L"Locate DHCP4 Service Binding : %r\n",
        Status
    );

    if (EFI_ERROR(Status))
    {
        return Status;
    }

    //
    // ========================================================
    // 2. 创建 DHCP4 Child
    // ========================================================
    //
    Print(L"\n[2] Create DHCP4 Child...\n");

    Status = Dhcp4Sb->CreateChild(
        Dhcp4Sb,
        &DhcpChild
    );

    Print(
        L"Create DHCP4 Child           : %r\n",
        Status
    );

    if (EFI_ERROR(Status))
    {
        return Status;
    }

    //
    // ========================================================
    // 3. 从 Child Handle 获取 EFI_DHCP4_PROTOCOL
    // ========================================================
    //
    Print(L"\n[3] Get EFI_DHCP4_PROTOCOL...\n");

    Status = gBS->HandleProtocol(
        DhcpChild,
        &gEfiDhcp4ProtocolGuid,
        (VOID **)&Dhcp4
    );

    Print(
        L"Get DHCP4 Protocol           : %r\n",
        Status
    );

    if (EFI_ERROR(Status))
    {
        goto CLEANUP;
    }

    //
    // ========================================================
    // 4. 配置 DHCP Discover 重试参数
    // ========================================================
    //
    Print(L"\n[4] Prepare DHCP4 Config...\n");

    Config.DiscoverTryCount = 4;

    Config.DiscoverTimeout = AllocateZeroPool(
        sizeof(UINT32) * Config.DiscoverTryCount
    );

    if (Config.DiscoverTimeout == NULL)
    {
        Print(L"Allocate DiscoverTimeout failed.\n");

        Status = EFI_OUT_OF_RESOURCES;

        goto CLEANUP;
    }

    //
    // 单位：秒
    //
    Config.DiscoverTimeout[0] = 2;
    Config.DiscoverTimeout[1] = 2;
    Config.DiscoverTimeout[2] = 4;
    Config.DiscoverTimeout[3] = 8;

    //
    // ========================================================
    // 5. 配置 DHCP Request 重试参数
    // ========================================================
    //
    Config.RequestTryCount = 4;

    Config.RequestTimeout = AllocateZeroPool(
        sizeof(UINT32) * Config.RequestTryCount
    );

    if (Config.RequestTimeout == NULL)
    {
        Print(L"Allocate RequestTimeout failed.\n");

        Status = EFI_OUT_OF_RESOURCES;

        goto CLEANUP;
    }

    Config.RequestTimeout[0] = 2;
    Config.RequestTimeout[1] = 2;
    Config.RequestTimeout[2] = 4;
    Config.RequestTimeout[3] = 8;

    //
    // ========================================================
    // 6. Configure DHCP4
    // ========================================================
    //
    Print(L"\n[5] Configure DHCP4...\n");

    Status = Dhcp4->Configure(
        Dhcp4,
        &Config
    );

    Print(
        L"Configure DHCP4              : %r\n",
        Status
    );

    if (EFI_ERROR(Status))
    {
        goto CLEANUP;
    }

    //
    // ========================================================
    // 7. 启动 DHCP
    //
    // 这里会执行：
    //
    // DHCPDISCOVER
    //      ↓
    // DHCPOFFER
    //      ↓
    // DHCPREQUEST
    //      ↓
    // DHCPACK
    //
    // ========================================================
    //
    Print(L"\n[6] Starting DHCP4...\n");

    Status = Dhcp4->Start(
        Dhcp4,
        NULL
    );

    Print(
        L"DHCP4 Start                  : %r\n",
        Status
    );

    if (EFI_ERROR(Status))
    {
        Print(L"\nDHCP FAILED!\n");

        goto CLEANUP;
    }

    //
    // ========================================================
    // 8. DHCP 成功以后读取当前 DHCP 状态
    // ========================================================
    //
    Print(L"\n[7] Get DHCP4 Mode Data...\n");

    Status = Dhcp4->GetModeData(
        Dhcp4,
        &Mode
    );

    Print(
        L"Get DHCP4 Mode               : %r\n",
        Status
    );

    if (EFI_ERROR(Status))
    {
        goto CLEANUP;
    }

    //
    // ========================================================
    // 9. 打印 DHCP 状态
    // ========================================================
    //
    Print(
        L"DHCP State                  : %d\n",
        Mode.State
    );

    //
    // ========================================================
    // 10. 打印获取到的 IP
    // ========================================================
    //
    Print(L"\n");
    Print(L"============================================\n");
    Print(L"DHCP SUCCESS!\n");
    Print(L"============================================\n");

    Print(
        L"Client IP   : %d.%d.%d.%d\n",
        Mode.ClientAddress.Addr[0],
        Mode.ClientAddress.Addr[1],
        Mode.ClientAddress.Addr[2],
        Mode.ClientAddress.Addr[3]
    );

    Print(
        L"Subnet Mask : %d.%d.%d.%d\n",
        Mode.SubnetMask.Addr[0],
        Mode.SubnetMask.Addr[1],
        Mode.SubnetMask.Addr[2],
        Mode.SubnetMask.Addr[3]
    );

    Print(
        L"Server IP   : %d.%d.%d.%d\n",
        Mode.ServerAddress.Addr[0],
        Mode.ServerAddress.Addr[1],
        Mode.ServerAddress.Addr[2],
        Mode.ServerAddress.Addr[3]
    );

    Print(L"============================================\n");


CLEANUP:

    //
    // ========================================================
    // 11. 停止 DHCP
    // ========================================================
    //
    if (Dhcp4 != NULL)
    {
        Dhcp4->Stop(
            Dhcp4
        );

        //
        // NULL 表示解除当前 DHCP4 配置
        //
        Dhcp4->Configure(
            Dhcp4,
            NULL
        );
    }

    //
    // ========================================================
    // 12. 释放 Timeout 数组
    // ========================================================
    //
    if (Config.DiscoverTimeout != NULL)
    {
        FreePool(
            Config.DiscoverTimeout
        );

        Config.DiscoverTimeout = NULL;
    }

    if (Config.RequestTimeout != NULL)
    {
        FreePool(
            Config.RequestTimeout
        );

        Config.RequestTimeout = NULL;
    }

    //
    // ========================================================
    // 13. 删除 DHCP Child
    // ========================================================
    //
    if (DhcpChild != NULL &&
        Dhcp4Sb != NULL)
    {
        Dhcp4Sb->DestroyChild(
            Dhcp4Sb,
            DhcpChild
        );
    }

    Print(L"\nDhcpTest finished: %r\n", Status);

    return Status;
}
