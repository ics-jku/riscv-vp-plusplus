#include "channel_slip.h"

#include <fcntl.h>
#include <linux/if.h>
#include <linux/if_tun.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <unistd.h>

// SLIP (as defined in RFC 1055) doesn't specify an MTU. We therefore
// subsequently allocate memory for the packet buffer using realloc(3).
#define SLIP_SNDBUF_STEP 1500

#define SLIP_END 0300
#define SLIP_ESC 0333
#define SLIP_ESC_END 0334
#define SLIP_ESC_ESC 0335

Channel_SLIP::~Channel_SLIP() {
	stop();
}

int Channel_SLIP::get_mtu(const char *dev) {
	struct ifreq ifr;

	memset(&ifr, 0, sizeof(ifr));
	strncpy(ifr.ifr_name, dev, IFNAMSIZ);

	int fd = socket(AF_INET, SOCK_DGRAM, 0);
	if (fd == -1) {
		throw std::system_error(errno, std::generic_category());
	}

	if (ioctl(fd, SIOCGIFMTU, (void *)&ifr) == -1) {
		close(fd);
		throw std::system_error(errno, std::generic_category());
	}

	close(fd);
	return ifr.ifr_mtu;
}

int Channel_SLIP::open_fd() {
	tunfd = open("/dev/net/tun", O_RDWR);
	if (tunfd == -1) {
		goto err0;
	}

	struct ifreq ifr;
	memset(&ifr, 0, sizeof(ifr));
	ifr.ifr_flags = IFF_TUN | IFF_NO_PI; /* read/write raw IP packets */
	strncpy(ifr.ifr_name, netdev.c_str(), IFNAMSIZ);
	if (ioctl(tunfd, TUNSETIFF, (void *)&ifr) == -1) {
		goto err1;
	}

	sndsiz = 0;
	if (!(sndbuf = (uint8_t *)malloc(SLIP_SNDBUF_STEP * sizeof(uint8_t)))) {
		goto err1;
	}
	rcvsiz = get_mtu(ifr.ifr_name);
	if (!(rcvbuf = (uint8_t *)malloc(rcvsiz * sizeof(uint8_t)))) {
		goto err2;
	}

	return tunfd;

err2:
	free(sndbuf);
	sndbuf = NULL;
err1:
	close(tunfd);
err0:
	std::system_error(errno, std::generic_category());
	tunfd = -1;

	return -1;
}

void Channel_SLIP::close_fd(int fd) {
	if (sndbuf) {
		free(sndbuf);
		sndbuf = NULL;
	}
	if (rcvbuf) {
		free(rcvbuf);
		rcvbuf = NULL;
	}

	if (tunfd > 0) {
		close(tunfd);
	}
}

void Channel_SLIP::send_packet(void) {
	if (tunfd < 0) {
		/* ignore */
		return;
	}

	ssize_t ret = write(tunfd, sndbuf, sndsiz);
	if (ret == -1) {
		throw std::system_error(errno, std::generic_category());
	} else if ((size_t)ret != sndsiz) {
		throw std::runtime_error("short write");
	}

	if (sndsiz > SLIP_SNDBUF_STEP && !(sndbuf = (uint8_t *)realloc(sndbuf, SLIP_SNDBUF_STEP))) {
		throw std::system_error(errno, std::generic_category());
	}
	sndsiz = 0;
}

void Channel_SLIP::handle_input(int fd) {
	ssize_t ret = read(fd, rcvbuf, rcvsiz);
	if (ret <= -1) {
		throw std::system_error(errno, std::generic_category());
	}

	for (size_t i = 0; i < static_cast<size_t>(ret); i++) {
		switch (rcvbuf[i]) {
			case SLIP_END:
				rxpush(SLIP_ESC);
				rxpush(SLIP_ESC_END);
				break;
			case SLIP_ESC:
				rxpush(SLIP_ESC);
				rxpush(SLIP_ESC_ESC);
				break;
			default:
				rxpush(rcvbuf[i]);
				break;
		}
	}
	rxpush(SLIP_END);
}

void Channel_SLIP::write_data(uint8_t data) {
	if (data == SLIP_END) {
		if (sndsiz > 0) {
			send_packet();
		}
		return;
	}

	if (sndsiz > 0 && sndbuf[sndsiz - 1] == SLIP_ESC) {
		switch (data) {
			case SLIP_ESC_END:
				sndbuf[sndsiz - 1] = SLIP_END;
				return;
			case SLIP_ESC_ESC:
				sndbuf[sndsiz - 1] = SLIP_ESC;
				return;
		}
	}

	if (sndsiz && sndsiz % SLIP_SNDBUF_STEP == 0) {
		size_t newsiz = (sndsiz + SLIP_SNDBUF_STEP) * sizeof(uint8_t);
		if (!(sndbuf = (uint8_t *)realloc(sndbuf, newsiz))) {
			throw std::system_error(errno, std::generic_category());
		}
	}
	sndbuf[sndsiz++] = data;
}
