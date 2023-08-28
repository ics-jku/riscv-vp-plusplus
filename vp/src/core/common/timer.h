#ifndef RISCV_VP_TIMER_H
#define RISCV_VP_TIMER_H

#include <pthread.h>
#include <stdbool.h>
#include <stdint.h>

#include <chrono>
#include <system_error>

class Timer {
   public:
	typedef std::chrono::duration<uint64_t, std::micro> usecs;

	typedef void (*Callback)(void *);
	class Context {
	   public:
		Timer::usecs duration;
		Callback fn;
		void *arg;

		Context(usecs _duration, Callback _fn, void *_arg) : duration(_duration), fn(_fn), arg(_arg){};
	};

	Timer(Callback fn, void *arg);
	~Timer(void);

	void pause(void);
	void start(usecs duration);

   private:
	Context ctx;
	pthread_t thread;
	bool running;

	void stop_thread(void);
};

#endif
