#include <iostream>
#include <unistd.h>
#include <thread>

class Timer{
	private:
		bool canceled;
		void start(int seconds, void (*callback)(void)) {
			sleep(seconds);
			if (!this->canceled){
				(*callback)();
			}
			delete this;
		}
	public:
		Timer(int seconds, void (*callback)(void)) {
			this->canceled = false;
			std::thread timerThread(&Timer::start, seconds, callback);
			//std::thread timerThread(this->start, seconds, callback);
			timerThread.detach();
		}
		~Timer() {
			std::cout << "delete called!!!" << std::endl;
		}
		void cancel() {
			this->canceled = true;
		}
};

void timer(int seconds) {
	sleep(seconds);
	std::cout << "OMG!!!!!" << std::endl;
}

int main(int srgc, char* srgv[]) {

	std::thread ti(timer, 3);
	ti.join();

	int c = 0;
	while (true) {
		sleep(1);
		std::cout << c << std::endl;
		c++;
	}
}
