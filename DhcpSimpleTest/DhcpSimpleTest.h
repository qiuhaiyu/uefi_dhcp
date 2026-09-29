DWORD IpToAddr(const char* ip)
{
	IN_ADDR addr{};

	int ret = InetPtonA(
		AF_INET,
		ip,
		&addr
	);

	if (ret != 1)
	{
		return INADDR_NONE;
	}

	return addr.S_un.S_addr;
}