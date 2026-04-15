/*
 * Copyright (C) 2026 Manfred Schlaegl <manfred.schlaegl@gmx.at>
 * based on https://github.com/agra-uni-bremen/riscv-vp/commits/master/vp/src/platform/common/slip.{h,cpp}
 */

#include "channel_slip.h"

#include <fcntl.h>
#include <linux/if.h>
#include <linux/if_tun.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <unistd.h>

/*
 * SLIP (as defined in RFC 1055) doesn't specify an MTU. We therefore
 * subsequently allocate memory for the packet buffer using realloc(3).
 */
#define SLIP_S2T_BUFFER_STEP 100

#define SLIP_END 0300      // (octal) = 192 (decimal) = 0xC0 (hex)
#define SLIP_ESC 0333      // (octal) = 219 (decimal) = 0xDB (hex)
#define SLIP_ESC_END 0334  // (octal) = 220 (decimal) = 0xDC (hex)
#define SLIP_ESC_ESC 0335  // (octal) = 221 (decimal) = 0xDD (hex)

Channel_SLIP::~Channel_SLIP() {
	stop();
}

int Channel_SLIP::get_mtu(const char *dev) {
	struct ifreq ifr;

	memset(&ifr, 0, sizeof(ifr));
	strncpy(ifr.ifr_name, dev, IFNAMSIZ);

	int fd = socket(AF_INET, SOCK_DGRAM, 0);
	if (fd == -1) {
		std::cerr << "CHANNEL_SLIP: ERROR: get_mtu: Failed to create socket (exception): " << strerror(errno)
		          << std::endl;
		throw std::system_error(errno, std::generic_category());
	}

	if (ioctl(fd, SIOCGIFMTU, (void *)&ifr) == -1) {
		std::cerr << "CHANNEL_SLIP: ERROR: get_mtu: Failed to read mtu (SIOCGIFMTU) (exception): " << strerror(errno)
		          << std::endl;
		close(fd);
		throw std::system_error(errno, std::generic_category());
	}

	close(fd);
	return ifr.ifr_mtu;
}

/************************************************
 * Channel_FD_IF
 ************************************************/

/* Channel_FD_IF open_fd: open tun and init */
int Channel_SLIP::open_fd() {
	/*
	 * setup tun
	 */
	tunfd = open("/dev/net/tun", O_RDWR);
	if (tunfd == -1) {
		std::cerr << "CHANNEL_SLIP: ERROR: Failed to open /dev/net/tun: " << strerror(errno) << std::endl;
		return -1;
	}

	struct ifreq ifr;
	memset(&ifr, 0, sizeof(ifr));
	ifr.ifr_flags = IFF_TUN | IFF_NO_PI; /* read/write raw IP packets */
	strncpy(ifr.ifr_name, netdev.c_str(), IFNAMSIZ);
	if (ioctl(tunfd, TUNSETIFF, (void *)&ifr) == -1) {
		std::cerr << "CHANNEL_SLIP: ERROR: Failed to open " << netdev << ": " << strerror(errno) << std::endl;
		goto err;
	}

	/*
	 * initialized slip2tun (s2t)
	 * buffer is allocated on demand
	 */
	s2t_buffer_size = 0;
	s2t_packet_size = 0;
	s2t_buffer = nullptr;
	s2t_state_esc = false;

	/* initialize tun2slip (t2s) */
	t2s_buffer_size = get_mtu(ifr.ifr_name);
	t2s_buffer = (uint8_t *)malloc(t2s_buffer_size * sizeof(uint8_t));
	if (t2s_buffer == nullptr) {
		std::cerr << "CHANNEL_SLIP: ERROR: Allocating tun2slip buffer: " << strerror(errno) << std::endl;
		goto err;
	}

	/* ok */
	return tunfd;

err:
	close(tunfd);
	tunfd = -1;
	return -1;
}

/* Channel_FD_IF close_fd: reset and close tun */
void Channel_SLIP::close_fd(int fd) {
	if (s2t_buffer) {
		free(s2t_buffer);
		s2t_buffer = nullptr;
	}
	if (t2s_buffer) {
		free(t2s_buffer);
		t2s_buffer = nullptr;
	}
	if (tunfd > 0) {
		close(tunfd);
		tunfd = -1;
	}
}

/* Channel_FD_IF write_data: from uart (slip) to outside (tun) */
void Channel_SLIP::write_data(uint8_t byte) {
	s2t(byte);
}

/* Channel_FD_IF handle_input: read from outside (tun) push to uart (slip) */
void Channel_SLIP::handle_input(int fd) {
	t2s(fd);
}

/************************************************
 * tun2slip
 ************************************************/

void Channel_SLIP::t2s(int fd) {
	/* read from tun */
	ssize_t ret = read(fd, t2s_buffer, t2s_buffer_size);
	if (ret <= -1) {
		std::cerr << "CHANNEL_SLIP: TUN2SLIP: WARNING: Reading from tun: " << strerror(errno) << " -> try again"
		          << std::endl;
		return;
	}

	/* convert packet to slip and push to uart */
	for (size_t i = 0; i < static_cast<size_t>(ret); i++) {
		switch (t2s_buffer[i]) {
			case SLIP_END:
				/* END -> ESC + ESC_END */
				rxpush(SLIP_ESC);
				rxpush(SLIP_ESC_END);
				break;
			case SLIP_ESC:
				/* ESC -> ESC -> ESC_ESC */
				rxpush(SLIP_ESC);
				rxpush(SLIP_ESC_ESC);
				break;
			default:
				/* push byte */
				rxpush(t2s_buffer[i]);
				break;
		}
	}

	/* packet complete */
	rxpush(SLIP_END);
}

/************************************************
 * slip2tun
 ************************************************/

void Channel_SLIP::s2t_send_packet(void) {
	if (tunfd < 0) {
		/* ignore */
		s2t_packet_size = 0;
		return;
	}

	if (s2t_packet_size == 0) {
		/* nothing to do */
		return;
	}

	ssize_t ret = write(tunfd, s2t_buffer, s2t_packet_size);
	if (ret < 0) {
		std::cerr << "CHANNEL_SLIP: SLIP2TUN: WARNING: Writing to tun: " << strerror(errno) << " -> ignored"
		          << std::endl;
	} else if ((size_t)ret != s2t_packet_size) {
		std::cerr << "CHANNEL_SLIP: SLIP2TUN: WARNING: Short write to tun -> ignored." << std::endl;
	}
}

void Channel_SLIP::s2t(uint8_t byte) {
	if (byte == SLIP_END) {
		/* packet complete -> send */
		s2t_send_packet();
		/* reset state for next packet */
		s2t_packet_size = 0;
		s2t_state_esc = false;
		return;
	}

	if (s2t_state_esc) {
		/* last byte was escape */
		if (byte == SLIP_ESC_END) {
			/* ESC + ESC_END -> END */
			byte = SLIP_END;
		} else if (byte == SLIP_ESC_ESC) {
			/* ESC + ESC_ESC -> ESC */
			byte = SLIP_ESC;
		} else {
			std::cerr << "CHANNEL_SLIP: SLIP2TUN: WARNING: Received invalid packet -> dropped" << std::endl;
			/* reset state for next packet & drop byte */
			s2t_packet_size = 0;
			s2t_state_esc = false;
			return;
		}
		/* esc handled */
		s2t_state_esc = false;

	} else if (byte == SLIP_ESC) {
		/* remember escape for handling of next byte & drop byte */
		s2t_state_esc = true;
		return;
	}

	/*
	 * adaptive s2t buffer
	 * if no more space -> grow by step size
	 * (never shrink)
	 */
	if (s2t_packet_size >= s2t_buffer_size) {
		s2t_buffer_size += SLIP_S2T_BUFFER_STEP;
		s2t_buffer = (uint8_t *)realloc(s2t_buffer, s2t_buffer_size);
		if (s2t_buffer == nullptr) {
			std::cerr << "CHANNEL_SLIP: SLIP2TUN: ERROR: (Re-)allocating slip2tun buffer (exception): "
			          << strerror(errno) << std::endl;
			throw std::system_error(errno, std::generic_category());
		}
	}

	/* add byte to buffer */
	s2t_buffer[s2t_packet_size++] = byte;
}
