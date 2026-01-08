#include "gdb_server.h"

#include <assert.h>
#include <err.h>
#include <libgdb/parser2.h>
#include <libgdb/response.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#include <systemc>

#include "platform/common/async_event.h"

extern std::map<std::string, GDBServer::packet_handler> handlers;

GDBServer::GDBServer(sc_core::sc_module_name name, std::vector<debug_target_if *> targets, DebugMemoryInterface *mm,
                     uint16_t port, bool cont_sim_on_wait, std::vector<mmu_memory_if *> mmus)
    : cont_sim_on_wait(cont_sim_on_wait) {
	(void)name;

	if (targets.size() <= 0)
		throw std::invalid_argument("no harts specified");
	if (mmus.size() > 0 && mmus.size() != targets.size())
		throw std::invalid_argument("invalid amount of MMUs specified");

	for (size_t i = 0; i < targets.size(); i++) {
		debug_target_if *target = targets.at(i);
		harts.push_back(target);

		events[target] = std::make_tuple(new sc_core::sc_event, (sc_core::sc_event *)NULL);
		if (mmus.size() > 0)
			mmu[target] = mmus.at(i);

		/* Don't block on WFI, otherwise the run_threads method
		 * does not work correctly. Allowing blocking WFI would
		 * likely increase the performance */
		target->block_on_wfi(false);
	}

	memory = mm;
	arch = harts.at(0)->get_architecture();  // assuming all harts use the same
	prevpkt = NULL;
	create_sock(port);

	SC_THREAD(run);

	thr = std::thread(&GDBServer::serve, this);
	thr.detach();
}

sc_core::sc_event *GDBServer::get_stop_event(debug_target_if *hart) {
	return std::get<0>(events.at(hart));
}

void GDBServer::set_run_event(debug_target_if *hart, sc_core::sc_event *event) {
	std::get<1>(events.at(hart)) = event;
}

void GDBServer::set_single_run(debug_target_if *hart, bool single) {
	single_run[hart] = single;
}

bool GDBServer::is_single_run(debug_target_if *hart) {
	auto it = single_run.find(hart);
	return it != single_run.end() && it->second;
}

void GDBServer::create_sock(uint16_t port) {
	struct sockaddr_in addr;
	int reuse;

	sockfd = socket(AF_INET, SOCK_STREAM, 0);
	if (sockfd == -1)
		throw std::system_error(errno, std::generic_category());

	reuse = 1;
	if (setsockopt(sockfd, SOL_TCP, TCP_NODELAY, &reuse, sizeof(reuse)) == -1)
		goto err;
	if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEPORT, &reuse, sizeof(reuse)) == -1)
		goto err;

	memset(&addr, 0, sizeof(addr));
	addr.sin_family = AF_INET;
	addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
	addr.sin_port = htons(port);

	if (bind(sockfd, (struct sockaddr *)&addr, sizeof(addr)) == -1)
		goto err;
	if (listen(sockfd, 0) == -1)
		goto err;

	return;
err:
	close(sockfd);
	throw std::system_error(errno, std::generic_category());
}

std::vector<debug_target_if *> GDBServer::get_harts(int id) {
	if (id == GDB_THREAD_ANY)
		id = 1; /* pick the first thread */

	if (id == GDB_THREAD_ALL)
		return harts;

	if (id < 1) /* thread ids should start at index 1 */
		throw std::out_of_range("Thread id is too small");
	else
		id--;

	std::vector<debug_target_if *> v;
	v.push_back(harts.at(id));
	return v;
}

uint64_t GDBServer::translate_addr(debug_target_if *hart, uint64_t addr, MemoryAccessType type) {
	mmu_memory_if *mmu_if;
	try {
		mmu_if = mmu.at(hart);
	} catch (const std::out_of_range &) {
		mmu_if = NULL;
	}

	if (!mmu_if) {
		return addr;
	} else {
		return mmu_if->v2p(addr, type);
	}
}

void GDBServer::exec_thread(thread_func fn, char op) {
	int thread;
	std::vector<debug_target_if *> threads;

	try {
		thread = thread_ops.at(op);
	} catch (const std::out_of_range &) {
		thread = GDB_THREAD_ANY;
	}

	threads = get_harts(thread);
	for (debug_target_if *thread : threads) fn(thread);
}

void GDBServer::run_all_harts(std::vector<debug_target_if *> harts_to_run) {
	if (harts_to_run.empty())
		return;
	// Start all harts
	sc_core::sc_event_or_list allharts;
	for (debug_target_if *hart : harts_to_run) {
		sc_core::sc_event *run_event = std::get<1>(events.at(hart));
		run_event->notify();
		allharts |= *std::get<0>(events.at(hart));
	}

	// Wait for any hart to stop or interrupt
	sc_core::sc_event_or_list all_events = allharts | interruptEvent;
	sc_core::wait(all_events);

	// Ensure all running harts are stopped
	std::vector<std::pair<debug_target_if *, CoreExecStatus>> orig_status;
	for (debug_target_if *hart : harts_to_run) {
		CoreExecStatus status = hart->get_status();
		orig_status.emplace_back(hart, status);

		if (status != CoreExecStatus::Runnable)
			continue;
		hart->set_status(CoreExecStatus::HitBreakpoint);

		sc_core::sc_event &stopev = *get_stop_event(hart);
		sc_core::wait(stopev);
	}

	// Restore original status
	for (auto &[hart, status] : orig_status) {
		hart->set_status(status);
	}
}

void GDBServer::writeall(int fd, char *data, size_t len) {
	ssize_t ret, w;

	w = 0;
	do {
		assert(len >= (size_t)w);
		ret = write(fd, &data[w], len - (size_t)w);
		if (ret < 0)
			throw std::system_error(errno, std::generic_category());

		w += ret;
	} while ((size_t)w < len);
}

void GDBServer::send_packet(int conn, const char *data, gdb_kind_t kind) {
	char *serialized;

	serialized = gdb_serialize(kind, data);

	try {
		writeall(conn, serialized, strlen(serialized));
	} catch (const std::system_error &e) {
		warnx("writeall failed: %s", e.what());
		goto ret;
	}

	/* acknowledgment are only used for GDB packets */
	if (kind != GDB_KIND_PACKET)
		goto ret;

	free(prevpkt);
	if (!(prevpkt = strdup(serialized))) {
		prevpkt = NULL;
		free(serialized);
		throw std::system_error(errno, std::generic_category());
	}

ret:
	free(serialized);
}

void GDBServer::retransmit(int conn) {
	if (!prevpkt)
		return;

	try {
		writeall(conn, prevpkt, strlen(prevpkt));
	} catch (const std::system_error &e) {
		warnx("writeall failed: %s", e.what());
	}

	/* memory for prevpkt is freed on next successfull
	 * packet transmit in the send_packet function */
}

void GDBServer::dispatch(int conn) {
	FILE *stream;
	gdb_packet_t *pkt;

	if (!(stream = fdopen(conn, "r")))
		throw std::system_error(errno, std::generic_category());

	for (;;) {
		if (!(pkt = gdb_parse_pkt(stream))) {
			break;
		}
		if (pkt->kind == GDB_KIND_INTERRUPT) {
			// Priority. Needs to be handled even if the systemC thread is blocked
			gdb_free_packet(pkt);
			interruptEvent.notify();
		} else {
			mtx.lock();
			pktq.push(std::make_tuple(conn, pkt));
			if (cont_sim_on_wait) {
				asyncEvent.notify();
			} else {
				cv.notify_one();
			}
			mtx.unlock();
		}
	}

	fclose(stream);
}

void GDBServer::serve(void) {
	int conn;

	for (;;) {
		if ((conn = accept(sockfd, NULL, NULL)) == -1) {
			warn("accept failed");
			continue;
		}

		dispatch(conn);
		close(conn);

		free(prevpkt);
		prevpkt = NULL;
	}
}

void GDBServer::run(void) {
	int conn;
	gdb_command_t *cmd;
	gdb_packet_t *pkt;
	packet_handler handler;

	for (;;) {
		std::unique_lock<std::mutex> lock(mtx);
		if (!pktq.empty()) {
			auto ctx = pktq.front();
			pktq.pop();
			lock.unlock();

			std::tie(conn, pkt) = ctx;

			switch (pkt->kind) {
				case GDB_KIND_NACK:
					retransmit(conn);
					/* fall through */
				case GDB_KIND_ACK:
					goto next1;
				default:
					break;
			}

			if (!(cmd = gdb_parse_cmd(pkt))) {
				send_packet(conn, NULL, GDB_KIND_NACK);
				goto next1;
			}

			send_packet(conn, NULL, GDB_KIND_ACK);
			try {
				handler = handlers.at(cmd->name);
			} catch (const std::out_of_range &) {
				// For any command not supported by the stub, an
				// empty response (‘$#00’) should be returned.
				send_packet(conn, "");
				goto next2;
			}

			(this->*handler)(conn, cmd);

		next2:
			gdb_free_cmd(cmd);
		next1:
			gdb_free_packet(pkt);
		} else {
			if (cont_sim_on_wait) {
				lock.unlock();
				sc_core::wait(asyncEvent);
			} else {
				cv.wait(lock);
				// Delta cycle needed to resync with SystemC scheduler after OS-level wait
				// TODO: why is this necessary?
				sc_core::wait(sc_core::SC_ZERO_TIME);
			}
		}
	}
}
