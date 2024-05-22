#include <iostream>
#include <unistd.h>
#include <thread>

class Timer{
	private:
		bool canceled;
		void (*callback)(void);

		void start(int seconds) {
			sleep(seconds);
			if (!this->canceled){
				(*this->callback)();
			}
			delete this;
		}
	public:
		Timer(int seconds, void (*callback)(void)) {
			this->canceled = false;
			this->callback = callback;
			std::thread timerThread([this, seconds]()-> void {
				this->start(seconds);
			});
			timerThread.detach();
		}
		~Timer() {
			std::cout << "delete called!!!" << std::endl;
		}
		void cancel() {
			this->canceled = true;
		}
		void force() {
			this->cancel();
			(*this->callback)();
		}
};

void timer(int seconds) {
	sleep(seconds);
	std::cout << "OMG!!!!!" << std::endl;
}

int main(int srgc, char* srgv[]) {

	void (*call)(void) = [](){
		system("notify-send test");
		std::cout << "!!!!OMG!!!!" << std::endl;
	};
	Timer* timer1 = new Timer(3, call);
	Timer* timer2 = new Timer(6, call);
	Timer* timer3 = new Timer(6, call);

	int c = 1;
	while (true) {
		sleep(1);
		if (c == 4) {
			timer2->force();
		}
		std::cout << c << std::endl;
		c++;
	}
}
