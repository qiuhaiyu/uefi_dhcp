#include <Uefi.h>

#include <Library/UefiLib.h>
#include <Library/UefiApplicationEntryPoint.h>
#include <Library/UefiBootServicesTableLib.h>

#include "../Include/MyProtocol.h"


//
// 注意：GUID 数值必须与 Driver 使用的一模一样
//
EFI_GUID gMyProtocolGuid = MY_PROTOCOL_GUID;


EFI_STATUS
EFIAPI
UefiMain(
    IN EFI_HANDLE        ImageHandle,
    IN EFI_SYSTEM_TABLE  *SystemTable
)
{
    EFI_STATUS   Status;
    MY_PROTOCOL  *MyProtocol = NULL;

    (VOID)ImageHandle;
    (VOID)SystemTable;

    Print(L"MyApp start...\n");

    Status = gBS->LocateProtocol(
        &gMyProtocolGuid,
        NULL,
        (VOID **)&MyProtocol
    );

    if (EFI_ERROR(Status))
    {
        Print(
            L"MyApp: LocateProtocol failed: %r\n",
            Status
        );

        return Status;
    }

    Print(L"MyApp: Protocol found!\n");

    Status = MyProtocol->Hello();

    if (EFI_ERROR(Status))
    {
        Print(
            L"MyApp: Hello failed: %r\n",
            Status
        );

        return Status;
    }

    Print(L"MyApp finished.\n");

    return EFI_SUCCESS;
}
