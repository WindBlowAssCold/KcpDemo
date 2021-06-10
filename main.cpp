#include "kcp.h"


DWORD StringToHash(char* lpszBuffer)
{
	DWORD dwHash = 0;
	while (*lpszBuffer)
	{
		dwHash = ((dwHash << 25) | (dwHash >> 7));
		dwHash = dwHash + *lpszBuffer;
		lpszBuffer++;
	}
	return dwHash;
}

int main()
{
	//CHAR str[] = "OutputDebugStringA";
	//DWORD hash = StringToHash(str);
	//printf("0x%X", hash);
	kcp m_kcp;
	char temp[] = "hello";
	char sendbuf[1024] = { 0 };
	char recvbuf[1024] = { 0 };

	m_kcp.start("10.2.0.28", 9999);

	long current;
	long slap = clock() + 1000;
	int index = 0;

	while (clock() < 60 * CLOCKS_PER_SEC)
	{
		Sleep(100);
		//m_kcp.update();

		current = clock();
		for (; current >= slap; slap += 2000)
		{
			sprintf(sendbuf, "%s %d", temp, index++);
			m_kcp.send(sendbuf, strlen(sendbuf));
			printf("[Send]:%s\n", sendbuf);
		}

		while (1) {
			int res = m_kcp.recv(recvbuf, sizeof(recvbuf));
			if (res < 0) break;
			printf("[Recv]:%s\n", recvbuf);
		}
	}

	m_kcp.end();
	return 0;
}