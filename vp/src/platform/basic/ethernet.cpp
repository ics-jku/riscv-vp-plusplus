#include "ethernet.h"

#include <netinet/ether.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/types.h>
// for debug
#include <arpa/inet.h>
#include <linux/if_packet.h>
#include <netinet/ip.h>
#include <netinet/udp.h>
//#include <netpacket/packet.h>
#include <net/if.h>
//#include <linux/if_ether.h>
#include <fcntl.h>
#include <ifaddrs.h>
#include <linux/if_tun.h>
#include <netdb.h>
#include <sys/ioctl.h>

using namespace std;

static const char IF_NAME[] = "tap0";

#define SYS_CHECK(arg, msg)           \
	if ((arg) < 0) {                  \
		perror(msg);                  \
		throw runtime_error("error"); \
	}

void printHex(const unsigned char *buf, const uint32_t len) {
	for (uint8_t i = 0; i < len; i++) {
		printf("%s%02X", i > 0 ? ":" : "", buf[i]);
	}
}

void printDec(const unsigned char *buf, const uint32_t len) {
	for (uint8_t i = 0; i < len; i++) {
		printf("%s%d", i > 0 ? "." : "", buf[i]);
	}
}

void dump_ethernet_frame(uint8_t *buf, size_t size, bool verbose = false) {
	uint8_t *readbuf = buf;
	struct ether_header *eh = (struct ether_header *)readbuf;

	if (verbose) {
		cout << "destination MAC: ";
		printHex(reinterpret_cast<unsigned char *>(&eh->ether_dhost), 6);
		cout << endl;
		cout << "source MAC     : ";
		printHex(reinterpret_cast<unsigned char *>(&eh->ether_shost), 6);
		cout << endl;
		cout << "type           : " << ntohs(eh->ether_type) << endl;
	}

	readbuf += sizeof(struct ethhdr);
	switch (ntohs(eh->ether_type)) {
		case ETH_P_IP: {
			cout << "IP ";
			struct in_addr source;
			struct in_addr dest;
			struct iphdr *ip = (struct iphdr *)readbuf;
			memset(&source, 0, sizeof(source));
			source.s_addr = ip->saddr;
			memset(&dest, 0, sizeof(dest));
			dest.s_addr = ip->daddr;
			if (verbose) {
				cout << endl;
				cout << "\t|-Version               : " << ip->version << endl;
				cout << "\t|-Internet Header Length: " << ip->ihl << " DWORDS or " << ip->ihl * 4 << " Bytes" << endl;
				cout << "\t|-Type Of Service       : " << (unsigned int)ip->tos << endl;
				cout << "\t|-Total Length          : " << ntohs(ip->tot_len) << " Bytes" << endl;
				cout << "\t|-Identification        : " << ntohs(ip->id) << endl;
				cout << "\t|-Time To Live          : " << (unsigned int)ip->ttl << endl;
				cout << "\t|-Protocol              : " << (unsigned int)ip->protocol << endl;
				cout << "\t|-Header Checksum       : " << ntohs(ip->check) << endl;
				cout << "\t|-Source IP             : " << inet_ntoa(source) << endl;
				cout << "\t|-Destination IP        : " << inet_ntoa(dest) << endl;
			}
			readbuf += ip->ihl * 4;
			switch (ip->protocol) {
				case IPPROTO_UDP: {
					cout << "UDP ";
					struct udphdr *udp = (struct udphdr *)readbuf;
					if (verbose) {
						cout << endl;
						cout << "\t|-Source port     : " << ntohs(udp->source) << endl;
						cout << "\t|-Destination port: " << ntohs(udp->dest) << endl;
						cout << "\t|-Length          : " << ntohs(udp->len) << endl;
						cout << "\t|-Checksum        : " << ntohs(udp->check) << endl;
					}
					readbuf += sizeof(udphdr);
					switch (ntohs(udp->dest)) {
						case 67:
						case 68:
							cout << "DHCP ";
							switch (readbuf[0]) {
								case 1:
									cout << "DISCOVER/REQUEST";
									break;
								case 2:
									cout << "ACK";
									break;
								default:
									cout << "UNKNOWN (" << to_string(readbuf[0]) << ")";
									goto printHex;
							}
							break;
						default:
							break;
					}
					return;
				}
				case IPPROTO_TCP:
					cout << "TCP ";
					if (verbose)
						cout << endl;
					return;
				case IPPROTO_ICMP:
					cout << "ICMP ";
					switch (readbuf[0]) {
						case 0:
							cout << "ECHO REPLY";
							break;
						case 3:
							cout << "DEST UNREACHABLE";
							break;
						case 8:
							cout << "ECHO REQUEST";
							break;
						default:
							cout << "Sonstiges";
					}
					if (verbose)
						cout << endl;
				default:
					return;
			}
			break;
		}
		case ETH_P_ARP: {
			cout << "ARP ";
			struct arp_eth_header *arp = (struct arp_eth_header *)readbuf;
			if (verbose) {
				cout << endl;
				cout << "\t|-Sender MAC: ";
				printHex((uint8_t *)&arp->sender_mac, 6);
				cout << endl;
				cout << "\t|-Sender IP : ";
				printDec((uint8_t *)&arp->sender_ip, 4);
				cout << endl;
				cout << "\t|-DEST MAC  : ";
				printHex((uint8_t *)&arp->target_mac, 6);
				cout << endl;
				cout << "\t|-DEST IP   : ";
				printDec((uint8_t *)&arp->target_ip, 4);
				cout << endl;
			}
			cout << "\t|-Operation : "
			     << (ntohs(arp->oper) == 1   ? "REQUEST"
			         : ntohs(arp->oper) == 2 ? "REPLY"
			                                 : "INVALID");
			return;
			break;
		}
		default:
			cout << "unknown protocol";
			return;
	}
printHex:
	ios_base::fmtflags f(cout.flags());
	for (unsigned i = 0; i < size; ++i) {
		cout << hex << setw(2) << setfill('0') << (int)buf[i] << " ";
	}
	cout.flags(f);
}

EthernetDevice::EthernetDevice(sc_core::sc_module_name, uint32_t irq_number, uint8_t *mem, std::string clonedev)
    : irq_number(irq_number), mem(mem) {
	tsock.register_b_transport(this, &EthernetDevice::transport);
	SC_THREAD(run);

	router
	    .add_register_bank({
	        {STATUS_REG_ADDR, &status},
	        {RECEIVE_SIZE_REG_ADDR, &receive_size},
	        {RECEIVE_DST_REG_ADDR, &receive_dst},
	        {SEND_SRC_REG_ADDR, &send_src},
	        {SEND_SIZE_REG_ADDR, &send_size},
	        {MAC_HIGH_REG_ADDR, &mac[0]},
	        {MAC_LOW_REG_ADDR, &mac[1]},
	    })
	    .register_handler(this, &EthernetDevice::register_access_callback);

	disabled = clonedev == string("");

	if (!disabled) {
		init_network(clonedev);
	}
}

void EthernetDevice::init_network(std::string clonedev) {
	struct ifreq ifr;
	int err;

	if ((sockfd = open(clonedev.c_str(), O_RDWR)) < 0) {
		cerr << "Error opening " << clonedev << endl;
		perror("exiting");
		assert(sockfd >= 0);
	}

	memset(&ifr, 0, sizeof(ifr));

	ifr.ifr_flags = IFF_TAP | IFF_NO_PI;

	strncpy(ifr.ifr_name, IF_NAME, IFNAMSIZ);

	if ((err = ioctl(sockfd, TUNSETIFF, (void *)&ifr)) < 0) {
		perror("ioctl(TUNSETIFF)");
		close(sockfd);
		assert(err >= 0);
	}

	/* Get the MAC address of the interface to send on */
	struct ifreq ifopts;
	memset(&ifopts, 0, sizeof(struct ifreq));
	strncpy(ifopts.ifr_name, IF_NAME, IFNAMSIZ - 1);
	if (ioctl(sockfd, SIOCGIFHWADDR, &ifopts) < 0) {
		perror("SIOCGIFHWADDR");
		assert(sockfd >= 0);
	}
	// Save own MAC in register
	memcpy(VIRTUAL_MAC_ADDRESS, ifopts.ifr_hwaddr.sa_data, 6);

	fcntl(sockfd, F_SETFL, O_NONBLOCK);
}

void EthernetDevice::send_raw_frame() {
	uint8_t sendbuf[send_size < 60 ? 60 : send_size];
	memcpy(sendbuf, &mem[send_src - 0x80000000], send_size);
	if (send_size < 60) {
		memset(&sendbuf[send_size], 0, 60 - send_size);
		send_size = 60;
	}

	cout << "SEND FRAME --->--->--->--->--->---> ";
	dump_ethernet_frame(sendbuf, send_size, true);
	cout << endl;

	struct ether_header *eh = (struct ether_header *)sendbuf;

	assert(memcmp(eh->ether_shost, VIRTUAL_MAC_ADDRESS, ETH_ALEN) == 0);

	ssize_t ans = write(sockfd, sendbuf, send_size);
	if (ans != send_size) {
		cout << strerror(errno) << endl;
	}
	assert(ans == send_size);
}

bool EthernetDevice::isPacketForUs(uint8_t *packet, ssize_t) {
	ether_header *eh = reinterpret_cast<ether_header *>(packet);
	bool virtual_match = memcmp(eh->ether_dhost, VIRTUAL_MAC_ADDRESS, ETH_ALEN) == 0;
	bool broadcast_match = memcmp(eh->ether_dhost, BROADCAST_MAC_ADDRESS, ETH_ALEN) == 0;
	bool own_packet = memcmp(eh->ether_shost, VIRTUAL_MAC_ADDRESS, ETH_ALEN) == 0;

	if (!virtual_match && !(broadcast_match && !own_packet)) {
		return false;
	}

	if (ntohs(eh->ether_type) != ETH_P_IP) {
		if (ntohs(eh->ether_type) != ETH_P_ARP) {  // Not ARP
			// cout << " dumped non-ARP " ;
			// FIXME change to true if you want other protocols than IP and ARP
			return false;
		}

		arp_eth_header *arp = reinterpret_cast<arp_eth_header *>(packet + sizeof(ether_header));

		if (memcmp(arp->target_mac, VIRTUAL_MAC_ADDRESS,
		           6)) {  // not to us directly
			// cout << " dumped ARP not targeted to US ";
			/**
			 * FIXME comment out if you want to generate arp table automatically
			 * 		 instead of having to request every neighbor explicitly
			 * 		 also to reply to ARP requests
			 */
			return true;
		}
	} else {
		// is IP
		return true;

		iphdr *ip = reinterpret_cast<iphdr *>(packet + sizeof(ether_header));
		if (ip->protocol != IPPROTO_UDP) {  // not UDP
			// cout << " dumped non-UDP ";
			// FIXME change to true if you want to use TCP or ICMP
			return false;
		}

		udphdr *udp = reinterpret_cast<udphdr *>(packet + sizeof(ether_header) + sizeof(iphdr));
		if (ntohs(udp->dest) != 67 && ntohs(udp->dest) != 68) {  // not DHCP
			// cout << " dumped non-DHCP ";
			return false;
		}
	}
	return true;
}

bool EthernetDevice::try_recv_raw_frame() {
	ssize_t ans;

	ans = read(sockfd, recv_frame_buf, FRAME_SIZE);
	assert(ans <= FRAME_SIZE);
	if (ans == 0) {
		cerr << "[ethernet] recv socket received zero bytes ... connection "
		        "closed?"
		     << endl;
		throw runtime_error("read failed");
	} else if (ans == -1) {
		if (errno == EWOULDBLOCK || errno == EAGAIN) {
			return true;
		} else
			throw runtime_error("recvfrom failed");
	}
	assert(ETH_ALEN == 6);

	if (!isPacketForUs(recv_frame_buf, ans)) {
		return false;
	}

	has_frame = true;
	receive_size = ans;
	cout << "RECEIVED FRAME <---<---<---<---<--- ";
	dump_ethernet_frame(recv_frame_buf, ans);
	cout << endl;

	return true;
}
