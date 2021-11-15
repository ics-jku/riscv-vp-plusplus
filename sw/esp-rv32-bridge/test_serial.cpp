#include <iostream>
#include <fstream>
#include <thread>
#include <mutex>
#include <queue>

std::mutex mtx;

class UART {
	private:
	std::queue<char> rx_fifo;
	std::queue<char> tx_fifo;

	std::fstream uart_stream;

	std::thread rx_thread;
	std::thread tx_thread;

	std::thread run_thread;

	public:
	UART(const std::string uart_fd_path) : uart_stream(uart_fd_path, std::fstream::in | std::fstream::out | std::fstream::app), run_thread(&UART::run, this)/*, rx_thread(&UART::run_rx, this), tx_thread(&UART::run_tx, this)*/ {
		
	}

	~UART(){
		//rx_thread.join();
		//tx_thread.join();
		run_thread.join();
	}

	private:
	void run_rx() {
		std::lock_guard<std::mutex> lock (mtx);
		std::cout << "run_rx" << std::endl;
		if (uart_stream.is_open()){
			std::cout << "[info] Reading from UART file stream"  << std::endl;

			char c;
			while (uart_stream.get(c)){
				std::cout << "pushing " << c << " into rx_fifo" << std::endl;
				rx_fifo.push(c);
				tx_fifo.push(c);
			}
		} else {
			std::cerr << "[error] Failed to open UART file stream" << std::endl;
		}
	}

	void run_tx(){
		std::lock_guard<std::mutex> lock (mtx);
		std::cout << "run_tx" << std::endl;
		if (uart_stream.is_open()){
			while(!tx_fifo.empty()){
				char c = tx_fifo.front();
				tx_fifo.pop();
				std::cout << "putting " << c << " into file" << std::endl;
				uart_stream.put(c);
				uart_stream.flush();
			}
			uart_stream.close();
		} else {
			// TODO implement
		}
	}

	void rx(){
		std::cout << "rx" << std::endl;
		if (uart_stream.is_open()){
			std::cout << "[info] Reading from UART file stream"  << std::endl;

			char c;
			while (uart_stream.get(c)){
				std::cout << "pushing " << c << " into rx_fifo" << std::endl;
				rx_fifo.push(c);
				tx_fifo.push(c);
			}
		} else {
			std::cerr << "[error] Failed to open UART file stream" << std::endl;
		}
	}

	void tx(){
		std::cout << "tx" << std::endl;
		if (uart_stream.is_open()){
			while(!tx_fifo.empty()){
				char c = tx_fifo.front();
				tx_fifo.pop();
				std::cout << "putting " << c << " into file" << std::endl;
				uart_stream.put(c);
				uart_stream.flush();
			}
			uart_stream.close();
		} else {
			// TODO implement
		}
	}

	void run(){
		rx();
		tx();
	}
};


int main(int argc, char **argv){
	std::string path(argv[1]);

	UART uart(path);

	return 0;
}
