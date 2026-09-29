#ifndef __MY_PROTOCOL_H__
#define __MY_PROTOCOL_H__

#include <Uefi.h>


//
// Protocol GUID
//
#define MY_PROTOCOL_GUID \
  { \
    0x5a0c1234, 0x1234, 0x4567, \
    { 0x89, 0xab, 0xcd, 0xef, 0x12, 0x34, 0x56, 0x78 } \
  }


//
// 前向声明
//
typedef struct _MY_PROTOCOL MY_PROTOCOL;


//
// Hello 函数类型
//
typedef
EFI_STATUS
(EFIAPI *MY_PROTOCOL_HELLO)(
    VOID
    );


//
// Protocol 本身
//
struct _MY_PROTOCOL
{
    MY_PROTOCOL_HELLO Hello;
};


//
// Protocol GUID 变量
//
extern EFI_GUID gMyProtocolGuid;


#endif

