#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>

#include <iostream>
#include <cstdio>
#include <cstring>
#include <cstddef>
#include "DhcpSimpleTest.h"

#pragma comment(lib, "ws2_32.lib")


// ============================================================
// DHCP 常量
// ============================================================

#define DHCP_SERVER_PORT 67
#define DHCP_CLIENT_PORT 68

#define DHCP_DISCOVER  1
#define DHCP_OFFER     2
#define DHCP_REQUEST   3
#define DHCP_DECLINE   4
#define DHCP_ACK       5
#define DHCP_NAK       6
#define DHCP_RELEASE   7
#define DHCP_INFORM    8

// ============================================================
// DHCP 报文
//
// BOOTP 固定部分：236 字节
// Magic Cookie：4 字节
// Options：变长
// ============================================================

#pragma pack(push, 1)

struct DHCP_PACKET
{
	BYTE op;                // 1 = BOOTREQUEST, 2 = BOOTREPLY
	BYTE htype;             // 1 = Ethernet
	BYTE hlen;              // MAC 长度，Ethernet = 6
	BYTE hops;

	DWORD xid;              // Transaction ID

	WORD secs;
	WORD flags;

	DWORD ciaddr;           // Client IP
	DWORD yiaddr;           // Your IP
	DWORD siaddr;           // Server IP
	DWORD giaddr;           // Relay Agent IP

	BYTE chaddr[16];        // Client MAC

	BYTE sname[64];         // Server Host Name
	BYTE file[128];         // Boot File Name

	BYTE magicCookie[4];    // 63 82 53 63

	BYTE options[312];
};

#pragma pack(pop)

// ============================================================
// 打印 MAC
// ============================================================

void PrintMac(const BYTE* mac)
{
	printf(
		"%02X:%02X:%02X:%02X:%02X:%02X",
		mac[0],
		mac[1],
		mac[2],
		mac[3],
		mac[4],
		mac[5]
	);
}

// ============================================================
// DHCP 类型转字符串
// ============================================================

const char* DhcpTypeToString(int type)
{
	switch (type)
	{
	case DHCP_DISCOVER:
		return "DHCP DISCOVER";

	case DHCP_OFFER:
		return "DHCP OFFER";

	case DHCP_REQUEST:
		return "DHCP REQUEST";

	case DHCP_DECLINE:
		return "DHCP DECLINE";

	case DHCP_ACK:
		return "DHCP ACK";

	case DHCP_NAK:
		return "DHCP NAK";

	case DHCP_RELEASE:
		return "DHCP RELEASE";

	case DHCP_INFORM:
		return "DHCP INFORM";

	default:
		return "UNKNOWN";
	}
}

// ============================================================
// 从 DHCP Options 中寻找 Option 53
//
// Option 格式：
//
//      Code + Length + Value
//
// Option 53:
//
//      53 01 xx
//
// xx:
//      1 = Discover
//      2 = Offer
//      3 = Request
//      5 = ACK
// ============================================================

int GetDhcpMessageType(
	const DHCP_PACKET* packet,
	int packetSize)
{
	const int fixedSize =
		static_cast<int>(
			offsetof(DHCP_PACKET, options)
			);

	if (packetSize <= fixedSize)
	{
		return 0;
	}

	const int optionSize =
		packetSize - fixedSize;

	int i = 0;

	while (i < optionSize)
	{
		// --------------------------------------------
		// 读取 Option Code
		// --------------------------------------------

		BYTE code =
			packet->options[i++];

		// Option 0 = PAD
		if (code == 0)
		{
			continue;
		}

		// Option 255 = END
		if (code == 255)
		{
			break;
		}

		// --------------------------------------------
		// 准备读取 Length
		// --------------------------------------------

		if (i >= optionSize)
		{
			break;
		}

		BYTE len =
			packet->options[i++];

		// --------------------------------------------
		// 防止 Value 越界
		// --------------------------------------------

		if (i + len > optionSize)
		{
			break;
		}

		// --------------------------------------------
		// Option 53 = DHCP Message Type
		// --------------------------------------------

		if (code == 53 && len == 1)
		{
			return packet->options[i];
		}

		// --------------------------------------------
		// 不是 Option 53
		//
		// 跳过 Value
		// --------------------------------------------

		i += len;
	}

	return 0;
}

// ============================================================
// 添加一个 DHCP Option
// ============================================================

void AddOption(
	BYTE* options,
	int& pos,
	BYTE code,
	BYTE len,
	const void* data)
{
	options[pos++] = code;
	options[pos++] = len;

	memcpy(
		&options[pos],
		data,
		len
	);

	pos += len;
}

// ============================================================
// 构造 DHCP OFFER / ACK
// ============================================================

int BuildDhcpReply(
	DHCP_PACKET* reply,
	const DHCP_PACKET* request,
	BYTE messageType,
	const char* clientIp,
	const char* serverIp)
{
	memset(
		reply,
		0,
		sizeof(DHCP_PACKET)
	);

	// ========================================================
	// BOOTP 固定字段
	// ========================================================

	reply->op = 2;       // BOOTREPLY
	reply->htype = 1;    // Ethernet
	reply->hlen = 6;

	// Transaction ID 必须与客户端一致
	reply->xid =
		request->xid;

	reply->secs =
		request->secs;

	reply->flags =
		request->flags;

	// ========================================================
	// 给客户端分配的 IP
	// ========================================================

	reply->yiaddr =
		IpToAddr(clientIp);

	// ========================================================
	// Server 地址
	// ========================================================

	reply->siaddr =
		IpToAddr(serverIp);

	// ========================================================
	// 客户端 MAC
	// ========================================================

	memcpy(
		reply->chaddr,
		request->chaddr,
		sizeof(reply->chaddr)
	);

	// ========================================================
	// DHCP Magic Cookie
	//
	// 必须是：
	//
	// 63 82 53 63
	// ========================================================

	reply->magicCookie[0] = 0x63;
	reply->magicCookie[1] = 0x82;
	reply->magicCookie[2] = 0x53;
	reply->magicCookie[3] = 0x63;

	// ========================================================
	// 开始构造 Options
	// ========================================================

	int pos = 0;

	// --------------------------------------------------------
	// Option 53
	// DHCP Message Type
	// --------------------------------------------------------

	AddOption(
		reply->options,
		pos,
		53,
		1,
		&messageType
	);

	// --------------------------------------------------------
	// Option 54
	// Server Identifier
	// --------------------------------------------------------

	DWORD serverAddress =
		IpToAddr(serverIp);

	AddOption(
		reply->options,
		pos,
		54,
		4,
		&serverAddress
	);

	// --------------------------------------------------------
	// Option 1
	// Subnet Mask
	//
	// 255.255.255.0
	// --------------------------------------------------------

	DWORD subnetMask =
		IpToAddr("255.255.255.0");

	AddOption(
		reply->options,
		pos,
		1,
		4,
		&subnetMask
	);

	// --------------------------------------------------------
	// Option 3
	// Router / Gateway
	//
	// 测试环境暂时把服务器地址当 Gateway
	// --------------------------------------------------------

	DWORD gateway =
		IpToAddr(serverIp);

	AddOption(
		reply->options,
		pos,
		3,
		4,
		&gateway
	);

	// --------------------------------------------------------
	// Option 51
	// Lease Time
	//
	// 3600 秒
	// --------------------------------------------------------

	DWORD leaseTime =
		htonl(3600);

	AddOption(
		reply->options,
		pos,
		51,
		4,
		&leaseTime
	);

	// --------------------------------------------------------
	// Option 58
	// Renewal Time
	//
	// 1800 秒
	// --------------------------------------------------------

	DWORD renewalTime =
		htonl(1800);

	AddOption(
		reply->options,
		pos,
		58,
		4,
		&renewalTime
	);

	// --------------------------------------------------------
	// Option 59
	// Rebinding Time
	//
	// 3150 秒
	// --------------------------------------------------------

	DWORD rebindingTime =
		htonl(3150);

	AddOption(
		reply->options,
		pos,
		59,
		4,
		&rebindingTime
	);

	// --------------------------------------------------------
	// Option 255
	// END
	// --------------------------------------------------------

	reply->options[pos++] = 255;

	// 返回真正需要发送的报文长度
	return static_cast<int>(
		offsetof(DHCP_PACKET, options)
		) + pos;
}

// ============================================================
// main
// ============================================================

int main()
{
	// ========================================================
	// 修改成你实际 DHCP Server 网卡的 IP
	// ========================================================

	const char* SERVER_IP =
		"192.168.1.168";

	// ========================================================
	// 当前教学版本固定给客户端这个 IP
	// ========================================================

	const char* OFFER_IP =
		"192.168.1.178";

	std::cout
		<< "========================================\n"
		<< " Simple DHCP Server\n"
		<< "========================================\n";

	std::cout
		<< "Server IP : "
		<< SERVER_IP
		<< std::endl;

	std::cout
		<< "Offer IP  : "
		<< OFFER_IP
		<< std::endl;

	// ========================================================
	// 初始化 Winsock
	// ========================================================

	WSADATA wsaData{};

	int ret =
		WSAStartup(
			MAKEWORD(2, 2),
			&wsaData
		);

	if (ret != 0)
	{
		std::cout
			<< "WSAStartup failed: "
			<< ret
			<< std::endl;

		return -1;
	}

	// ========================================================
	// 创建 UDP Socket
	// ========================================================

	SOCKET sock =
		socket(
			AF_INET,
			SOCK_DGRAM,
			IPPROTO_UDP
		);

	if (sock == INVALID_SOCKET)
	{
		std::cout
			<< "socket() failed: "
			<< WSAGetLastError()
			<< std::endl;

		WSACleanup();

		return -1;
	}

	// ========================================================
	// 开启广播
	// ========================================================

	BOOL enableBroadcast = TRUE;

	ret =
		setsockopt(
			sock,
			SOL_SOCKET,
			SO_BROADCAST,
			reinterpret_cast<const char*>(
				&enableBroadcast
				),
			sizeof(enableBroadcast)
		);

	if (ret == SOCKET_ERROR)
	{
		std::cout
			<< "setsockopt(SO_BROADCAST) failed: "
			<< WSAGetLastError()
			<< std::endl;
	}

	// ========================================================
	// 绑定 UDP 67
	// ========================================================

	sockaddr_in localAddr{};

	localAddr.sin_family =
		AF_INET;

	localAddr.sin_port =
		htons(DHCP_SERVER_PORT);

	localAddr.sin_addr.s_addr =
		INADDR_ANY;

	ret =
		bind(
			sock,
			reinterpret_cast<sockaddr*>(
				&localAddr
				),
			sizeof(localAddr)
		);

	if (ret == SOCKET_ERROR)
	{
		std::cout
			<< "bind UDP 67 failed. error = "
			<< WSAGetLastError()
			<< std::endl;

		std::cout
			<< "Please run Visual Studio / EXE as Administrator."
			<< std::endl;

		closesocket(sock);
		WSACleanup();

		return -1;
	}

	std::cout
		<< "\nDHCP Server started."
		<< std::endl;

	std::cout
		<< "Listening UDP port 67..."
		<< std::endl;

	// ========================================================
	// 主循环
	// ========================================================

	while (true)
	{
		DHCP_PACKET request{};

		sockaddr_in remoteAddr{};

		int remoteAddrLen =
			sizeof(remoteAddr);

		// ====================================================
		// 等待 DHCP 数据
		// ====================================================

		int recvLen =
			recvfrom(
				sock,
				reinterpret_cast<char*>(
					&request
					),
				sizeof(request),
				0,
				reinterpret_cast<sockaddr*>(
					&remoteAddr
					),
				&remoteAddrLen
			);

		if (recvLen == SOCKET_ERROR)
		{
			std::cout
				<< "recvfrom() failed: "
				<< WSAGetLastError()
				<< std::endl;

			continue;
		}

		std::cout
			<< "\n----------------------------------------"
			<< std::endl;

		std::cout
			<< "Receive DHCP packet"
			<< std::endl;

		std::cout
			<< "Packet size : "
			<< recvLen
			<< std::endl;

		// ====================================================
		// 检查最基本的长度
		// ====================================================

		if (recvLen <
			static_cast<int>(
				offsetof(DHCP_PACKET, options)
				))
		{
			std::cout
				<< "Invalid DHCP packet."
				<< std::endl;

			continue;
		}

		// ====================================================
		// 检查 Magic Cookie
		// ====================================================

		if (request.magicCookie[0] != 0x63 ||
			request.magicCookie[1] != 0x82 ||
			request.magicCookie[2] != 0x53 ||
			request.magicCookie[3] != 0x63)
		{
			std::cout
				<< "Invalid DHCP Magic Cookie."
				<< std::endl;

			continue;
		}

		// ====================================================
		// 打印 Transaction ID
		// ====================================================

		printf(
			"Transaction ID : 0x%08X\n",
			ntohl(request.xid)
		);

		// ====================================================
		// 打印 MAC
		// ====================================================

		std::cout
			<< "Client MAC     : ";

		PrintMac(request.chaddr);

		std::cout << std::endl;

		// ====================================================
		// 获取 Option 53
		// ====================================================

		int messageType =
			GetDhcpMessageType(
				&request,
				recvLen
			);

		std::cout
			<< "Message Type   : "
			<< messageType
			<< " ("
			<< DhcpTypeToString(messageType)
			<< ")"
			<< std::endl;

		// ====================================================
		// 准备返回包
		// ====================================================

		DHCP_PACKET reply{};

		sockaddr_in destination{};

		destination.sin_family =
			AF_INET;

		destination.sin_port =
			htons(DHCP_CLIENT_PORT);

		// 客户端此时可能还没有 IP
		// 所以使用广播
		destination.sin_addr.s_addr =
			INADDR_BROADCAST;

		// ====================================================
		// DISCOVER
		//      ↓
		// OFFER
		// ====================================================

		if (messageType == DHCP_DISCOVER)
		{
			std::cout
				<< "Client sends DHCP DISCOVER."
				<< std::endl;

			int sendLen =
				BuildDhcpReply(
					&reply,
					&request,
					DHCP_OFFER,
					OFFER_IP,
					SERVER_IP
				);

			int result =
				sendto(
					sock,
					reinterpret_cast<const char*>(
						&reply
						),
					sendLen,
					0,
					reinterpret_cast<sockaddr*>(
						&destination
						),
					sizeof(destination)
				);

			if (result == SOCKET_ERROR)
			{
				std::cout
					<< "Send DHCP OFFER failed: "
					<< WSAGetLastError()
					<< std::endl;
			}
			else
			{
				std::cout
					<< "Send DHCP OFFER"
					<< std::endl;

				std::cout
					<< "Offer IP : "
					<< OFFER_IP
					<< std::endl;
			}
		}

		// ====================================================
		// REQUEST
		//      ↓
		// ACK
		// ====================================================

		else if (messageType == DHCP_REQUEST)
		{
			std::cout
				<< "Client sends DHCP REQUEST."
				<< std::endl;

			int sendLen =
				BuildDhcpReply(
					&reply,
					&request,
					DHCP_ACK,
					OFFER_IP,
					SERVER_IP
				);

			int result =
				sendto(
					sock,
					reinterpret_cast<const char*>(
						&reply
						),
					sendLen,
					0,
					reinterpret_cast<sockaddr*>(
						&destination
						),
					sizeof(destination)
				);

			if (result == SOCKET_ERROR)
			{
				std::cout
					<< "Send DHCP ACK failed: "
					<< WSAGetLastError()
					<< std::endl;
			}
			else
			{
				std::cout
					<< "Send DHCP ACK"
					<< std::endl;

				std::cout
					<< "Assigned IP : "
					<< OFFER_IP
					<< std::endl;
			}
		}
		else
		{
			std::cout
				<< "Ignore DHCP message."
				<< std::endl;
		}
	}

	// 实际上当前 while(true) 不会执行到这里
	closesocket(sock);

	WSACleanup();

	return 0;
}