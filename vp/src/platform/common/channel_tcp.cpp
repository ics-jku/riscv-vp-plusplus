#include "channel_tcp.h"

#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

#include <iostream>

Channel_TCP::Channel_TCP(unsigned int port) : port(port) {
	/* create server socket */
	serverfd = socket(AF_INET, SOCK_STREAM, 0);
	if (serverfd < 0) {
		std::cerr << "Channel_TCP: error opening socket" << std::endl;
		throw std::system_error(errno, std::generic_category());
	}

	/* bind server socket */
	sockaddr_in saddr;
	saddr.sin_family = AF_INET;
	saddr.sin_port = htons(port);
	saddr.sin_addr.s_addr = INADDR_ANY;  // listen to all ip addresse
	if (bind(serverfd, (struct sockaddr*)&saddr, sizeof(saddr)) < 0) {
		std::cerr << "Channel_TCP: error binding socket" << std::endl;
		close(serverfd);
		serverfd = -1;
		throw std::system_error(errno, std::generic_category());
	}
}

Channel_TCP::~Channel_TCP() {
	close(serverfd);
	serverfd = -1;
	stop();
}

int Channel_TCP::open_fd() {
	if (serverfd < 0) {
		return -1;
	}

	/* listen on server socket -> block until client connects */
	std::cout << "Channel_TCP: Listening on port " << port << std::endl;
	if (listen(serverfd, 1) < 0) {
		close(serverfd);
		serverfd = -1;
		return -1;
	}

	/* accept client */
	clientfd = accept(serverfd, nullptr, nullptr);
	if (clientfd < 0) {
		close(serverfd);
		serverfd = -1;
		clientfd = -1;
		return -1;
	}

	std::cout << "Channel_TCP: Client connected on port " << port << std::endl;
	return clientfd;
}

void Channel_TCP::close_fd(int fd) {
	close(clientfd);
	clientfd = -1;
	std::cout << "Channel_TCP: Client disconnected from port " << port << std::endl;
}

void Channel_TCP::handle_input(int fd) {
	uint8_t byte;
	// TODO: handle larger chunks
	ssize_t ret = read(fd, &byte, 1);
	if (ret <= -1) {
		throw std::system_error(errno, std::generic_category());
	}
	rxpush(byte);
}

void Channel_TCP::write_data(uint8_t data) {
	if (clientfd < 0) {
		return;
	}
	write(clientfd, &data, 1);
}
