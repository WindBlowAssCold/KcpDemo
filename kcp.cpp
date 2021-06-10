#include "kcp.h"
#include <algorithm>

uint64_t unix_timestamp_ms() {
	auto now = std::chrono::high_resolution_clock::now();
	auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count();
	return ms;
}

int udp_send(const char* buf, int len, IKCPCB* kcp, void* user)
{
	sockinfo* sock_info = (sockinfo*)user;
	return sendto(sock_info->sock, buf, len, 0, (sockaddr*)&sock_info->addr, sizeof(sock_info->addr));
}

kcp::kcp()
{
	sock_info = { 0 };
	thread_exit_ = false;
	/* init kcp */
	kcp_ = ikcp_create(get_conv(), &sock_info);
	kcp_->stream = 1;
	kcp_->output = udp_send;
}

kcp::~kcp() {
	ikcp_release(kcp_);
}

struct IKCPCB* kcp::raw_kcp() const {
	return kcp_;
}

int kcp::recv(char* buffer, int len) {
	std::lock_guard<std::mutex> __lock(mutex_);
	int n = ikcp_recv(kcp_, buffer, len);
	return n;
}

int kcp::send(const char* buffer, int len) {
	std::lock_guard<std::mutex> __lock(mutex_);
	int n = ikcp_send(kcp_, buffer, len);
	return n;
}

int kcp::peek_size() {
	std::lock_guard<std::mutex> __lock(mutex_);
	int n = ikcp_peeksize(kcp_);
	return n;
}

uint32_t kcp::check() {
	std::lock_guard<std::mutex> __lock(mutex_);
	return ikcp_check(kcp_, current_ts());
}

bool kcp::writable() {
	std::lock_guard<std::mutex> __lock(mutex_);

	IUINT32 cwnd = min(kcp_->snd_wnd, kcp_->rmt_wnd);
	if (kcp_->nocwnd == 0) {
		cwnd = min(kcp_->cwnd, cwnd);
	}
	bool b = ikcp_waitsnd(kcp_) < cwnd;
	//    bool b = ikcp_waitsnd(kcp_) < 2 * kcp_->snd_wnd;
	return b;
}

int kcp::input(const uint8_t* data, size_t size) {
	std::lock_guard<std::mutex> __lock(mutex_);
	int n = ikcp_input(kcp_, (const char*)data, size);
	return n;
}

void kcp::update() {
	std::lock_guard<std::mutex> __lock(mutex_);
	ikcp_update(kcp_, current_ts());
}

uint32_t kcp::current_ts() const {
	uint32_t current = static_cast<uint32_t>(unix_timestamp_ms() & 0xfffffffful);
	return current;
}

/***********************************************************************************/

uint32_t kcp::get_conv()
{
	return GetConvByMac();
}

void kcp::end()
{
	/* release thread */
	thread_exit_ = true;
	update_thread.join();
	recv_thread.join();

	/* release socket */
	closesocket(sock_info.sock);
	WSACleanup();
}

bool kcp::start(const char* ip, int port)
{
	/* init socket */
	WORD wVersionrequested;
	WSADATA wsaData;

	wVersionrequested = MAKEWORD(2, 2);
	if (WSAStartup(wVersionrequested, &wsaData) != 0)
	{
		return false;
	}

	sock_info.sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
	if (sock_info.sock == INVALID_SOCKET)
	{
		WSACleanup();
		return false;
	}

	memset(&sock_info.addr, 0, sizeof(struct sockaddr));
	sock_info.addr.sin_family = AF_INET;
	sock_info.addr.sin_port = htons(port);
	sock_info.addr.sin_addr.s_addr = inet_addr(ip);

	int imode = 1;
	int nRet = ioctlsocket(sock_info.sock, FIONBIO, (u_long*)&imode);
	if (nRet == SOCKET_ERROR)
	{
		closesocket(sock_info.sock);
		WSACleanup();
		return false;
	}

	/* if server */
	//if (bind(sock_, (sockaddr*)&addr, sizeof(sockaddr)) == SOCKET_ERROR)
	//{
	//	return -1;
	//}

	/* init thread */
	recv_thread = std::thread(&kcp::udp_recv_thread, this);
	update_thread = std::thread(&kcp::kcp_update_thread, this);

	update();
}

void kcp::udp_recv_thread()
{
	/* recv UDP packet */
	char buf[1024] = { 0 };
	int addr_len = sizeof(sockaddr);
	while (!thread_exit_)
	{
		while (1) {
			int res = recvfrom(sock_info.sock, buf, sizeof(buf), 0, (sockaddr*)&sock_info.toaddr, &addr_len);
			if (res < 0)
				break;
			input((UCHAR*)buf, res);
		}
	}
}

void kcp::kcp_update_thread()
{
	while (!thread_exit_)
	{
		int time = check() - current_ts();
		Sleep(time > 0 ? time : 0);
		update();
	}
}