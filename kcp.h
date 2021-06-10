#ifndef KCPP_KCP_H
#define KCPP_KCP_H

#include <cstdint>
#include <mutex>
#include <atomic>
#include "ikcp.h"
#include "util.h"

typedef struct sockinfo
{
	SOCKET sock;
	sockaddr_in addr;
	sockaddr_in toaddr;
};

class kcp {

public:
	/* kcp */
	typedef int (*output_t)(const char* buf, int len, struct IKCPCB* kcp, void* user);
	kcp();
	~kcp();
	struct IKCPCB* raw_kcp() const;
	int recv(char* buffer, int len);
	int send(const char* buffer, int len);
	int peek_size();
	uint32_t check();
	bool writable();
	int input(const uint8_t* data, size_t size);
	void update();
	//    void set_nodelay(int nodelay, int interval, int resend, int nc);
	//    void set_wndsize(int sndwnd, int rcvwnd);

public:
	/* socket */
	uint32_t get_conv();
	bool start(const char* ip, int port);
	void end();
	/* thread */
	void udp_recv_thread();
	void kcp_update_thread();

private:
	uint32_t current_ts() const;
	struct IKCPCB* kcp_;
	sockinfo sock_info;
	std::mutex mutex_;
	std::thread recv_thread;
	std::thread update_thread;
	bool thread_exit_;
};
#endif //KCPP_KCP_H
