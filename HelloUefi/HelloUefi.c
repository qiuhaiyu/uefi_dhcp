#include <Uefi.h>
#include <Library/UefiLib.h>
#include <Library/UefiApplicationEntryPoint.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/UefiRuntimeServicesTableLib.h>
#include <Library/MemoryAllocationLib.h>


#include <Protocol/GraphicsOutput.h>
#include <Protocol/BlockIo.h>
#include <Library/BaseLib.h>
#include <Guid/Gpt.h>
#include <Library/BaseMemoryLib.h>

#pragma pack(push,1)

typedef struct
{
    UINT64 Signature;

    UINT32 Revision;
    UINT32 HeaderSize;
    UINT32 HeaderCRC32;
    UINT32 Reserved;

    UINT64 MyLBA;
    UINT64 AlternateLBA;

    UINT64 FirstUsableLBA;
    UINT64 LastUsableLBA;

    EFI_GUID DiskGUID;

    UINT64 PartitionEntryLBA;

    UINT32 NumberOfPartitionEntries;
    UINT32 SizeOfPartitionEntry;

    UINT32 PartitionEntryArrayCRC32;

} MY_GPT_HEADER;


typedef struct
{
    EFI_GUID PartitionTypeGUID;       // 16 bytes
    EFI_GUID UniquePartitionGUID;     // 16 bytes

    UINT64 StartingLBA;               // 8 bytes
    UINT64 EndingLBA;                 // 8 bytes

    UINT64 Attributes;                // 8 bytes

    CHAR16 PartitionName[36];         // 72 bytes

} MY_GPT_ENTRY;




typedef struct
{
    // 跳转指令
    // CPU 执行引导扇区代码时使用的跳转指令
    // FAT32 Boot Sector 偏移：0x00，长度：3 字节
    UINT8   JumpBoot[3];

    // OEM 名称/标识
    // 通常是创建该 FAT 文件系统的工具或系统标识
    // 偏移：0x03，长度：8 字节
    UINT8   OEMName[8];


    // 每个扇区的字节数
    // 常见值：512
    // 偏移：0x0B，长度：2 字节
    UINT16  BytesPerSector;

    // 每个簇包含多少个扇区
    // 例如：8 表示一个 Cluster = 8 个 Sector
    // 偏移：0x0D，长度：1 字节
    UINT8   SectorsPerCluster;

    // 保留扇区数量
    // 从 FAT32 分区开始到第一个 FAT 表之前的扇区数量
    // FAT32 常见值：32
    // 偏移：0x0E，长度：2 字节
    UINT16  ReservedSectorCount;

    // FAT 表数量
    // 通常为 2，即 FAT1 + FAT2
    // 偏移：0x10，长度：1 字节
    UINT8   NumberOfFATs;

    // 根目录项数量
    // FAT12/FAT16 使用
    // FAT32 中这个字段必须为 0
    // 偏移：0x11，长度：2 字节
    UINT16  RootEntryCount;

    // 总扇区数量（16 位）
    // 如果总扇区数量无法用 16 位表示，这里为 0，
    // 使用下面的 TotalSectors32
    // FAT32 中通常为 0
    // 偏移：0x13，长度：2 字节
    UINT16  TotalSectors16;

    // 媒体描述符
    // 硬盘通常为 0xF8
    // 偏移：0x15，长度：1 字节
    UINT8   Media;

    // 每个 FAT 表占用的扇区数（16 位版本）
    // FAT12/FAT16 使用
    // FAT32 中这个字段必须为 0
    // FAT32 使用后面的 FATSize32
    // 偏移：0x16，长度：2 字节
    UINT16  FATSize16;

    // 每磁道扇区数
    // 来自传统 CHS 磁盘参数
    // 现代系统中主要为了兼容
    // 偏移：0x18，长度：2 字节
    UINT16  SectorsPerTrack;

    // 磁头数量
    // 同样属于传统 CHS 参数
    // 偏移：0x1A，长度：2 字节
    UINT16  NumberOfHeads;

    // 隐藏扇区数量
    // 通常表示从整个磁盘开始到当前 FAT32 分区开始之间有多少个扇区
    // 可以理解为当前分区的起始 LBA（在常见分区磁盘场景下）
    // 偏移：0x1C，长度：4 字节
    UINT32  HiddenSectors;

    // 总扇区数量（32 位）
    // 当 TotalSectors16 == 0 时使用这里
    // FAT32 通常使用这个字段
    // 偏移：0x20，长度：4 字节
    UINT32  TotalSectors32;


    // 一个 FAT 表占用多少个扇区
    // 这是 FAT32 使用的字段
    // 偏移：0x24，长度：4 字节
    UINT32  FATSize32;

    // FAT32 扩展标志
    // 用于控制 FAT 镜像以及当前活动 FAT 等
    // 偏移：0x28，长度：2 字节
    UINT16  ExtFlags;

    // FAT32 文件系统版本
    // 通常为 0x0000
    // 偏移：0x2A，长度：2 字节
    UINT16  FileSystemVersion;

    // 根目录所在的起始簇号
    // FAT32 通常为 2
    // 偏移：0x2C，长度：4 字节
    UINT32  RootCluster;

    // FSInfo 结构所在的扇区号
    // 相对于 FAT32 分区起始位置
    // 常见值：1
    // 偏移：0x30，长度：2 字节
    UINT16  FSInfo;

    // 备份 Boot Sector 所在的扇区号
    // 相对于 FAT32 分区起始位置
    // 常见值：6
    // 偏移：0x32，长度：2 字节
    UINT16  BackupBootSector;

} FAT32_BPB;

typedef struct
{
    UINT8   Name[11];              // 0x00

    UINT8   Attributes;            // 0x0B

    UINT8   NTReserved;            // 0x0C
    UINT8   CreationTimeTenth;     // 0x0D

    UINT16  CreationTime;          // 0x0E
    UINT16  CreationDate;          // 0x10

    UINT16  LastAccessDate;        // 0x12

    UINT16  FirstClusterHigh;      // 0x14

    UINT16  WriteTime;             // 0x16
    UINT16  WriteDate;             // 0x18

    UINT16  FirstClusterLow;       // 0x1A

    UINT32  FileSize;              // 0x1C

} FAT_DIR_ENTRY;

#pragma pack(pop)

//辅助函数，获取下一个 cluster
EFI_STATUS
GetNextCluster(
	EFI_BLOCK_IO_PROTOCOL *BlockIo,
	FAT32_BPB *Bpb,
	EFI_LBA Fat1DiskLba,
	UINT32 CurrentCluster,
	UINT32 *NextCluster

){
	EFI_STATUS Status;
	UINT64 FatOffset;
	UINT64 FatSector;
	UINTN EntryOffset;
	UINT8 *Buffer;
	// FAT32 每个 FAT Entry 占 4 字节
	FatOffset = (UINT64)CurrentCluster * 4;
	// 这个 FAT Entry 位于 FAT 的第几个 Sector
	FatSector = FatOffset / Bpb->BytesPerSector;
	// 位于这个 Sector 内部的哪个位置
	EntryOffset = (UINTN)(FatOffset % Bpb->BytesPerSector);
	Buffer = AllocateZeroPool(Bpb->BytesPerSector);
	if(Buffer == NULL){
		return EFI_OUT_OF_RESOURCES;
	}
	Status = BlockIo->ReadBlocks(BlockIo,BlockIo->Media->MediaId,Fat1DiskLba + FatSector,Bpb->BytesPerSector,Buffer);
	if(EFI_ERROR(Status)){
		FreePool(Buffer);
		return Status;
	}
	*NextCluster = (*(UINT32 * )(Buffer + EntryOffset))&0x0FFFFFFF;
	FreePool(Buffer);
	return EFI_SUCCESS;



}

EFI_STATUS
ClusterToDiskLba(
	FAT32_BPB *Bpb,
	EFI_LBA PartitionStartLba,
	UINT32 Cluster
){
	UINT64 FirstDataSector;
	UINT64 ClusterSector;
	// Data Area 相对于分区的起始 Sector
	FirstDataSector = Bpb->ReservedSectorCount + ((UINT64)Bpb->NumberOfFATs * Bpb->FATSize32);

	// Cluster 2 对应 Data Area 的第一个 Cluster
	ClusterSector = FirstDataSector + ((UINT64)(Cluster - 2)*Bpb->SectorsPerCluster);

	return PartitionStartLba + ClusterSector;

}

//文件读取函数
EFI_STATUS
ReadFat32File(
	EFI_BLOCK_IO_PROTOCOL *BlockIo,
	FAT32_BPB *Bpb,
	EFI_LBA PartitionStartLba,
	EFI_LBA Fat1DiskLba,
	UINT32 FirstCluster,
	UINT32 FileSize
){

	EFI_STATUS Status;
	UINT32 CurrentCluster;
	UINT32 NextCluster;

	UINTN ClusterSize;
	UINT8 *ClusterBuffer;

	UINT32 BytesRemaining;
	UINTN BytesToPrint;

	ClusterSize = Bpb->BytesPerSector * Bpb->SectorsPerCluster;
	ClusterBuffer = AllocateZeroPool(ClusterSize);

	if(ClusterBuffer == NULL){
		return EFI_OUT_OF_RESOURCES;
	}

	CurrentCluster = FirstCluster;
	BytesRemaining = FileSize;

	Print(L"\n===== file Content ====\n");

	while(BytesRemaining >0 ){

		EFI_LBA ClusterDiskLba;
		ClusterDiskLba = ClusterToDiskLba(Bpb,PartitionStartLba,CurrentCluster);
		Print(L"\n [Cluster %d,Disk LBA %ld]\n",CurrentCluster,ClusterDiskLba);

		Status = BlockIo->ReadBlocks(BlockIo,BlockIo->Media->MediaId,ClusterDiskLba,ClusterSize,ClusterBuffer);

		if(EFI_ERROR(Status)){
			Print(L"Read Cluster %d failed: %r \n",CurrentCluster,Status);
			FreePool(ClusterBuffer);
			return Status;
		}

		if(BytesRemaining > ClusterSize){
			BytesToPrint = ClusterSize;
		}else{
			BytesToPrint = BytesRemaining;
		}

		for(UINTN i = 0; i< BytesToPrint; i++){
			Print(L"%c", ClusterBuffer[i]);
		}

		BytesRemaining -= (UINT32)BytesToPrint;

		if(BytesRemaining == 0){
			break;
		}

		// 5. 查询 FAT：下一个 Cluster 是谁？
		Status = GetNextCluster(
			BlockIo,Bpb,Fat1DiskLba,CurrentCluster,&NextCluster
		);

		if(EFI_ERROR(Status)){
			Print(L"\nGetNext Cluster failed:%r\n",Status);
			FreePool(ClusterBuffer);
			return Status;
		}

		Print(L"\nFAT[%d]->ox%08x\n",CurrentCluster,NextCluster);

		if(NextCluster >= 0x0FFFFFF8){
			Print(L"\nEnd of chain\n");
			break;
		}

		if(NextCluster<2){
			Print(L"\n Invalid next cluster: %d\n",NextCluster);
			FreePool(ClusterBuffer);
			return EFI_VOLUME_CORRUPTED;
		}
		CurrentCluster = NextCluster;


	}
	Print(L"\n\n ==== End File ===== \n");
	FreePool(ClusterBuffer);
	return EFI_SUCCESS;


}








EFI_STATUS
EFIAPI
UefiMain(
  IN EFI_HANDLE        ImageHandle,
  IN EFI_SYSTEM_TABLE* SystemTable
)
{
    EFI_STATUS Status;
	EFI_HANDLE *HandleBuffer = NULL;
	UINTN HandleCount = 0;
	Status = gBS->LocateHandleBuffer(
		ByProtocol,
		&gEfiBlockIoProtocolGuid,
		NULL,
		&HandleCount,
		&HandleBuffer
		
	);


	if(EFI_ERROR(Status))
	{
		Print(L"locateHandleBuffer faild: %r\n",Status);
		return Status;

	}

//屏幕分辨率
	EFI_GRAPHICS_OUTPUT_PROTOCOL *Gop;
	Status = gBS->LocateProtocol(
		&gEfiGraphicsOutputProtocolGuid,
		NULL,
		(VOID **)&Gop
	);

	if(EFI_ERROR(Status))
	{
		Print(L"Locate GOP faild: %r\n",Status);
		return Status;
	}
	for(UINT32 Mode = 0; Mode<Gop->Mode->MaxMode;Mode++){
		EFI_GRAPHICS_OUTPUT_MODE_INFORMATION * Info;
		UINTN SizeOfInfo;
		Status = Gop->QueryMode(Gop,Mode,&SizeOfInfo,&Info);
		if (EFI_ERROR(Status))
		{
			Print(L"QueryMode failed: %r\n", Status);
			continue;
		}
		
		Print(L"Mode %d: %dx%d\n",Mode,Info->HorizontalResolution,Info->VerticalResolution);

		if(Info->HorizontalResolution == 1920 && Info->VerticalResolution == 1080){
			//Status = Gop->SetMode(Gop,Mode);
			Print(L"Set 1920*1080: %r\n",Status);
			break;
		}
		

		gBS->FreePool(Info);
		
	}
	
#if 1
	Print(L"Count:%d \n",HandleCount);

	for(UINTN i = 0;i<HandleCount; i++)
    {
        
        EFI_BLOCK_IO_PROTOCOL *BlockIo = NULL;
        Status = gBS->OpenProtocol(
            HandleBuffer[i],
            &gEfiBlockIoProtocolGuid,
            (VOID**)&BlockIo,
            ImageHandle,
            NULL,
            EFI_OPEN_PROTOCOL_GET_PROTOCOL
        );

        

        if(EFI_ERROR(Status))
        {
            Print(
                L"OpenProtocol [%d] faild: %r\n",
                i,
                Status
            );

            continue;

        }

       if(BlockIo->Media->LogicalPartition == FALSE && i == 1){
       
                Print(L"\n=== Block Device %d ===\n", i);
        
                Print(L"Handle      : %p\n", HandleBuffer[i]);
        
                Print(L"BlockSize : %d bytes\n",
                      BlockIo->Media->BlockSize);
        
                Print(L"LastBlock : %ld\n",
                      BlockIo->Media->LastBlock);
        
                Print(L"MediaId   : %d\n",
                      BlockIo->Media->MediaId);
        
                Print(L"LogicalPartition   : %d\n",
                      BlockIo->Media->LogicalPartition);
        
                Print(L"RemovableMedia     : %d\n",
                      BlockIo->Media->RemovableMedia);
                Print(L"MediaPresent   : %d\n",
                      BlockIo->Media->MediaPresent);

                UINT32 BlockSize;
                BlockSize = BlockIo->Media->BlockSize;
                UINT8 *Buffer;
                Buffer = AllocateZeroPool(BlockSize);
                if(Buffer == NULL){
                    Print(L"Allocate buffer faild\n");
                    continue;
                }

//                Status = BlockIo->ReadBlocks(BlockIo,BlockIo->Media->MediaId,0,BlockSize,Buffer);
//                if(EFI_ERROR(Status)){
//                    Print(
//                        L"read LBA0 faild: %r\n",
//                        Status
//                    );
//                    FreePool(Buffer);
//                    continue;
//                }

//                if(Buffer[510] == 0x55 && Buffer[511] == 0xAA){
//                    Print(L"valid MBR Signature :: 55 AA \n");
//                }else{
//                    Print(L"Invalid MBR Signature\n");
//                }

				Status = BlockIo->ReadBlocks(BlockIo,BlockIo->Media->MediaId,1,BlockSize,Buffer);

				if (EFI_ERROR(Status))
				{
				    Print(L"Read GPT Header failed: %r\n", Status);

				    FreePool(Buffer);
				    continue;
				}

				if (Buffer[0] != 'E' ||
				    Buffer[1] != 'F' ||
				    Buffer[2] != 'I' ||
				    Buffer[3] != ' ' ||
				    Buffer[4] != 'P' ||
				    Buffer[5] != 'A' ||
				    Buffer[6] != 'R' ||
				    Buffer[7] != 'T')
				{
				    Print(L"Not GPT Header\n");

				    FreePool(Buffer);
				    continue;
				}

				MY_GPT_HEADER *GptHeader;

				GptHeader = (MY_GPT_HEADER *)Buffer;

				Print(
				    L"PartitionEntryLBA : %ld\n",
				    GptHeader->PartitionEntryLBA
				);

				Print(
				    L"NumberOfPartitionEntries : %ld\n",
				    GptHeader->NumberOfPartitionEntries
				);

				Print(
				    L"SizeOfPartitionEntry : %ld\n",
				    GptHeader->SizeOfPartitionEntry
				);


				UINTN k;

				Print(L"GPT Signature bytes: ");

				for (k = 0; k < 8; k++)
				{
				    Print(L"%02x ", Buffer[k]);
				}

				Print(L"\n");

				//读取 partitionEnterLBA

				UINTN EntryBytes;
				UINTN EntryBlockCount;
				UINTN EntryReadSize;
				UINT8 *EntryBuffer;


				// 1. GPT Entry Array实际数据大小
				EntryBytes =
				    GptHeader->NumberOfPartitionEntries *
				    GptHeader->SizeOfPartitionEntry;


				// 2. 需要读取多少个Block
				EntryBlockCount =
				    (EntryBytes + BlockSize - 1) / BlockSize;
				Print(L"EntryBlockCount:%d",EntryBlockCount);

				// 3. ReadBlocks真正读取的字节数
				EntryReadSize =
				    EntryBlockCount * BlockSize;

				EntryBuffer = AllocateZeroPool(EntryReadSize);


				Status = BlockIo->ReadBlocks(
				    BlockIo,
				    BlockIo->Media->MediaId,
				    GptHeader->PartitionEntryLBA,
				    EntryReadSize,
				    EntryBuffer
				);

				if (EFI_ERROR(Status))
				{
				    Print(L"Read GPT Entry Array failed: %r\n", Status);

				    FreePool(EntryBuffer);
				    FreePool(Buffer);

				    continue;
				}

				for (UINTN q = 0;
				     q < EntryBlockCount;
				     q++)
				{
				    MY_GPT_ENTRY *Entry;

				    Entry = (MY_GPT_ENTRY *)(
				        EntryBuffer +
				        q * GptHeader->SizeOfPartitionEntry
				    );

					if(IsZeroGuid(&Entry->PartitionTypeGUID)){
							continue;
					}

				    Print(L"\nPartition Entry[%d]\n", q);

				    Print(
				        L"StartingLBA : %ld\n",
				        Entry->StartingLBA
				    );

				    Print(
				        L"EndingLBA   : %ld\n",
				        Entry->EndingLBA
				    );

				    Print(
				        L"Type GUID   : %g\n",
				        &Entry->PartitionTypeGUID
				    );

				    Print(
				        L"Unique GUID : %g\n",
				        &Entry->UniquePartitionGUID
				    );

				    Print(
				        L"Name        : %s\n",
				        Entry->PartitionName
				    );

					
					if (CompareGuid(
							&Entry->PartitionTypeGUID,
							&gEfiPartTypeSystemPartGuid))
					{
						Print(L"This is EFI System Partition\n");
					}
					else
					{
						Print(L"This is NOT EFI System Partition\n");
					}

					UINT8 *PartBuffer;
					//UINTN x;
					PartBuffer = AllocateZeroPool(BlockSize);

					if (PartBuffer == NULL)
					{
						Print(L"Allocate PartBuffer failed\n");
						continue;
					}

					Status = BlockIo->ReadBlocks(
						BlockIo,
						BlockIo->Media->MediaId,
						Entry->StartingLBA,
						BlockSize,
						PartBuffer
					);

					if (EFI_ERROR(Status))
					{
						Print(L"Read partition first block failed: %r\n", Status);

						FreePool(PartBuffer);
						continue;
					}

					Print(
						L"\nRead Partition First Block Success, Disk LBA = %ld\n",
						Entry->StartingLBA
					);

					Print(L"\n=== FAT32 BPB ===\n");

					FAT32_BPB *Bpb;

					Bpb = (FAT32_BPB *)PartBuffer;

					Print(
						L"BytesPerSector       : %d\n",
						Bpb->BytesPerSector
					);

					Print(
						L"SectorsPerCluster    : %d\n",
						Bpb->SectorsPerCluster
					);

					Print(
						L"ReservedSectorCount  : %d\n",
						Bpb->ReservedSectorCount
					);

					Print(
						L"NumberOfFATs         : %d\n",
						Bpb->NumberOfFATs
					);

					Print(
						L"TotalSectors32       : %d\n",
						Bpb->TotalSectors32
					);

					Print(
						L"FATSize32            : %d\n",
						Bpb->FATSize32
					);

					Print(
						L"RootCluster          : %d\n",
						Bpb->RootCluster
					);

					Print(
						L"FSInfo               : %d\n",
						Bpb->FSInfo
					);

					Print(
						L"BackupBootSector     : %d\n",
						Bpb->BackupBootSector
					);

					// for (x = 0; x < BlockSize; x++)
					// {
					// 	if (x % 16 == 0)
					// 	{
					// 		Print(L"\n%04x: ", x);
					// 	}

					// 	Print(L"%02x ", PartBuffer[x]);
					// 	// 每 20 行暂停一次
					// 	if ((x + 1) % (16 * 20) == 0)
					// 	{
					// 		EFI_INPUT_KEY Key;
					// 		UINTN Index;

					// 		Print(L"\n\nPress any key to continue...\n");

					// 		gBS->WaitForEvent(
					// 			1,
					// 			&SystemTable->ConIn->WaitForKey,
					// 			&Index
					// 		);

					// 		SystemTable->ConIn->ReadKeyStroke(
					// 			SystemTable->ConIn,
					// 			&Key
					// 		);
					// 	}
					// }

					Print(L"\n");

					UINT64 FirstDataSector;
					UINT64 RootDirSector;
					UINT64 RootDirDiskLba;
					FirstDataSector = Bpb->ReservedSectorCount + Bpb->NumberOfFATs * Bpb->FATSize32;
					RootDirSector = FirstDataSector + (Bpb->RootCluster - 2) * Bpb->SectorsPerCluster;
					RootDirDiskLba = Entry->StartingLBA + RootDirSector;

					Print(L"RootDirDiskLba:%lu",RootDirDiskLba);

					UINT8 *RootBuffer;
					UINTN RootReadSize;
					RootReadSize = Bpb->SectorsPerCluster * Bpb->BytesPerSector;
					RootBuffer = AllocateZeroPool(RootReadSize);
					if(RootBuffer == NULL)
					{
						Print(L"allocate rootbuffer failed \n");
						continue;
					}

					Status = BlockIo->ReadBlocks(BlockIo,BlockIo->Media->MediaId,RootDirDiskLba,RootReadSize,RootBuffer);
					if(EFI_ERROR(Status)){

						Print(L"read Root directory failded: %r\n",Status);
					}
#if 0
					UINTN x1;
					for (x1 = 0; x1 < RootReadSize; x1++)
					{
						if (x1 % 16 == 0)
						{

							Print(L"\n%04x: ", x1);
						}
						Print(L"%02x ", RootBuffer[x1]);

						if ((x1 + 1) % (16 * 20) == 0)
						{
							EFI_INPUT_KEY Key;
							UINTN Index;

							Print(L"\n\nPress any key to continue...\n");

							gBS->WaitForEvent(
								1,
								&SystemTable->ConIn->WaitForKey,
								&Index);

							SystemTable->ConIn->ReadKeyStroke(
								SystemTable->ConIn,
								&Key);
						}
					}
#endif

					//FAT 表
					UINT64 Fat1DiskLba;
					UINT8 *FatBuffer;
					Fat1DiskLba = Entry->StartingLBA + Bpb->ReservedSectorCount;
					Print(L"\nFAT #1 Disk LBA : %ld\n",Fat1DiskLba);
					FatBuffer = AllocateZeroPool(Bpb->BytesPerSector);
					if(FatBuffer == NULL){
						Print(L"Allocate FatBuffer failed\n");
						continue;
					}
					Status = BlockIo->ReadBlocks(BlockIo,BlockIo->Media->MediaId,Fat1DiskLba,Bpb->BytesPerSector,FatBuffer);
					if(EFI_ERROR(Status)){
						Print(L"read Fat #1 failed: %r\n",Status);
						FreePool(FatBuffer);
						continue;
					}
					Print(L"Read FAT #1 success\n");
					UINT32 *FatEntries;
					FatEntries = (UINT32*)FatBuffer;
					for(UINTN x2 = 0;x2<16; x2++){
						Print(L"FAT[%d] = 0x%08x\n",x2,FatEntries[x2]);
					}



					UINTN EntryCount;
					UINTN n;
					EntryCount = RootReadSize / sizeof(FAT_DIR_ENTRY);

					Print(L"EntryCount:%d\n",EntryCount);
					for(n = 0; n<EntryCount ; n++){
						FAT_DIR_ENTRY * DirEntry;
						DirEntry = (FAT_DIR_ENTRY * )(RootBuffer + n* sizeof(FAT_DIR_ENTRY));
						//表示后面没有目录项了
						if(DirEntry->Name[0] == 0x00){break;}
						//0xe5 表示这个目录项已删除
						if(DirEntry->Name[0] == 0xE5){
							continue;
						}
						Print(L"\nDirectory Entry[%d]\n",n);
						Print(L"Name:");
						for(UINTN m = 0;m < 11; m++){
							Print(L"%c",DirEntry->Name[m]);
						}
						Print(L"\n");
						Print(L"Attributes:0x%02x\n",DirEntry->Attributes);
						Print(L"FileSize  : %d \n",DirEntry->FileSize);

						// 获取 文件里面的数据
						UINT32 FirstCluster;
						UINT64 FirstSectorOfCluster;
						UINT64 FileDiskLba;

						FirstCluster = ((UINT32)DirEntry->FirstClusterHigh << 16) | DirEntry->FirstClusterLow;
						FirstSectorOfCluster = FirstDataSector + ((UINT64)(FirstCluster - 2) * Bpb->SectorsPerCluster);
						FileDiskLba = Entry->StartingLBA + FirstSectorOfCluster;
						Print(L"FirstCluster         : %d\n", FirstCluster);
						Print(L"FirstSectorOfCluster : %ld\n", FirstSectorOfCluster);
						Print(L"File Disk LBA        : %ld\n", FileDiskLba);

						UINTN ClusterSize;
						ClusterSize  = Bpb->SectorsPerCluster * Bpb->BytesPerSector;
						UINT8 *FileBuffer;
						FileBuffer = AllocateZeroPool(ClusterSize);
						if(FileBuffer == NULL){
							Print(L"Allocate FileBuffer failed\n");
							continue;
						}
						Status = BlockIo->ReadBlocks(BlockIo,BlockIo->Media->MediaId,FileDiskLba,ClusterSize,FileBuffer);
						if(EFI_ERROR(Status)){

							Print(L"Read file cluster failed: %r\n",Status);
							FreePool(FileBuffer);
							continue;
						}
						Print(L"\nFile Content:\n");
						for(UINTN m=0; m<DirEntry->FileSize;m++){
							//Print(L"%c",FileBuffer[m]);
						}
						Print(L"\n");
						UINT32 NextCluster;
						Status = GetNextCluster(BlockIo,Bpb,Fat1DiskLba,FirstCluster,&NextCluster);
						if (!EFI_ERROR(Status))
						{
							Print(
								L"FAT[%d] = 0x%08x\n",
								FirstCluster,
								NextCluster);
						}

						// Status = ReadFat32File(
						// 	BlockIo,
						// 	Bpb,
						// 	Entry->StartingLBA,
						// 	Fat1DiskLba,
						// 	FirstCluster,
						// 	DirEntry->FileSize);

						// if (EFI_ERROR(Status))
						// {
						// 	Print(
						// 		L"ReadFat32File failed: %r\n",
						// 		Status);
						// }
					}

					

					FreePool(PartBuffer);



				}
				

                

                Print(L"\n");
                FreePool(Buffer);

            
        }
        
        

        

        

    }
    FreePool(HandleBuffer);
	
	
#endif
  return EFI_SUCCESS;

}
