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

class Daemon{
	private:
		std::string openCommand;
		std::string closeCommand;
		std::string updateCommand;
		std::vector<std::string> options;

		bool isOpen;
		void open() {
			system(this->openCommand.c_str());
			this->isOpen = true;
		}
	public:
		Daemon() {
			// init with the json config file
			this->isOpen = false;
			this->openCommand = "";
			this->closeCommand = "";
			this->updateCommand = "";
			// options = ????
		}
		void update(std::string newState) {
			if (!this->isOpen){
				this->open();
			}
			system(std::format(this->updateCommand, newState).c_str()); 
		}
		void close() {
			system(this->closeCommand.c_str());
			this->isOpen = false;
		}
};

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
			std::thread timerThread(this->start, seconds, callback);
			timerThread.detach();
		}
		~Timer() {
			std::cout << "delete called!!!" << std::endl;
		}
		void cancel() {
			this->canceled = true;
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
