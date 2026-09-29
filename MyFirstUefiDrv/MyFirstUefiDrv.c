#include <Uefi.h>

#include <Library/UefiLib.h>
#include <Library/UefiBootServicesTableLib.h>

#include "../Include/MyProtocol.h"


//
// 定义 Protocol GUID
//
EFI_GUID gMyProtocolGuid = MY_PROTOCOL_GUID;


//
// Protocol 的 Hello 函数
//
EFI_STATUS
EFIAPI
MyProtocolHello(
    VOID
)
{
    Print(L"Hello from MyFirstUefiDrv!\n");

    return EFI_SUCCESS;
}


//
// 创建 Protocol 实例
//
MY_PROTOCOL gMyProtocol =
{
    MyProtocolHello
};


EFI_STATUS
EFIAPI
MyDriverEntry(
    IN EFI_HANDLE        ImageHandle,
    IN EFI_SYSTEM_TABLE  *SystemTable
)
{
    EFI_STATUS Status;

    (VOID)SystemTable;

    Print(L"MyFirstUefiDrv loading...\n");


    //
    // 将我们自己的 Protocol 安装到 ImageHandle
    //
    Status = gBS->InstallProtocolInterface(
        &ImageHandle,
        &gMyProtocolGuid,
        EFI_NATIVE_INTERFACE,
        &gMyProtocol
        );


    if (EFI_ERROR(Status))
    {
        Print(
            L"InstallProtocolInterface failed: %r\n",
            Status
            );

        return Status;
    }


    Print(L"MyFirstUefiDrv loaded successfully!\n");

    return EFI_SUCCESS;
}
