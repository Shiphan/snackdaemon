#include <iostream>
#include <csignal>
#include <vector>
#include <string>
#include <format>
#include <thread>

void printHelp() {
	std::cout << "I am not helping you lol. (TODO)" << std::endl;
}
void printErrorArgs() {
	std::cout << "worng arguments, try \"daemon help\" to get help." << std::endl;
}
void closeSnackbar();

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
		}
		void cancel() {
			this->canceled = true;
		}
		void force() {
			this->cancel();
			(*this->callback)();
		}
};

class Daemon{
	private:
		std::string openCommand;
		std::string closeCommand;
		std::string updateCommand;
		std::vector<std::string> options;

		bool isOpen;
		Timer* timer;

	public:
		Daemon() {
			this->isOpen = false;
			this->timer = nullptr;

			// init with the json config file
			this->openCommand = "";
			this->closeCommand = "";
			this->updateCommand = "";
			// options = ????
		}
		void update(std::string newState) {
			if (!this->isOpen){
				this->isOpen = true;
				system(this->openCommand.c_str());
			} else {
				this->timer->cancel();
			}
			// newState string to index
			int index;
			system(std::format(this->updateCommand, index).c_str()); 
			this->timer = new Timer(2, [this]()->void {
				system(this->closeCommand.c_str());
			});
		}
		void close() {
			if (this->isOpen) {
				this->timer->force();
				this->isOpen = false;
				this->timer = nullptr;
			}
		}
};


int main(int argc, char* argv[]) {
	bool daemonRunning = false; //use shared memory
	switch (argc) {
		case 1:
			printHelp();
			break;
		case 2:
			if (argv[1] == std::string("daemon")) {
				// if daemon is runing, say it
				// else 
				// start the daemon
			} else if (argv[1] == std::string("kill")) {
				// if daemon is not runing, say it
				// else
				// kill the daemon
			} else if (argv[1] == std::string("close")) {
				// if daemon is not runing, say it
				// else
				// close the snackbar
			} else if (argv[1] == std::string("help")) {
				printHelp();
			} else {
				printErrorArgs();
			}
			break;
		case 3:
			if (argv[1] == std::string("update")) {
				// if daemon is not runing, say it
				// else
				// store arg to shared memory
			} else {
				printErrorArgs();
			}
			break;
		default:
			printErrorArgs();
	}
}

/*
daemon daemon:		start daemon
daemon kill:		kill daemon
daemon update arg:	update state (and open if not yet)
daemon close:		close
*/
