#include <iostream>
#include <vector>
#include <string>
#include <thread>
#include <strstream>

#include <csignal>

std::string format(std::string format, std::vector<std::string> values) {
	std::ostrstream strstream;
	int valuesIndex = 0;
	for (int i = 0; i < format.length(); i++) {
		if (format.at(i) == '\\' && format.length() > i+1 && format.at(i+1) == '{') {
			i++;
			strstream << "{";
		} else if (format.at(i) == '{' && format.length() > i+1 && format.at(i+1) == '}') {
			if (valuesIndex < values.size()) {
				strstream << values.at(valuesIndex);
				valuesIndex++;
				i++;
			} else {
				strstream << "{}";
				i++;
			}
		} else {
			strstream << format[i];
		}
	}
	return strstream.str();
}

class Timer{
	private:
		bool canceled;

		template<typename Callable>
		void start(int seconds, Callable callback) {
			sleep(seconds);
			if (!this->canceled){
				callback();
			}
			delete this;
		}
	public:
		template<typename Callable>
		Timer(int seconds, Callable callback) {
			this->canceled = false;
			std::thread timerThread([this, seconds, callback]()-> void {
				this->start(seconds, callback);
			});
			timerThread.detach();
		}
		~Timer() {
		}
		void cancel() {
			this->canceled = true;
		}
};

class Daemon{
	private:
		const std::string openCommand;
		const std::string closeCommand;
		const std::string updateCommand;
		const std::vector<std::string> options;

		bool isOpen;
		Timer* timer;

	public:
		Daemon() :openCommand(""), closeCommand(""), updateCommand(""), options({}) {
			this->isOpen = false;
			this->timer = nullptr;
		}
		Daemon(std::string openCommand, std::string closeCommand, std::string updateCommand, std::vector<std::string> options)
		:openCommand(openCommand), closeCommand(closeCommand), updateCommand(updateCommand), options(options) {
			this->isOpen = false;
			this->timer = nullptr;
		}
		void update(std::string newState) {
			if (!this->isOpen){
				this->isOpen = true;
				system(this->openCommand.c_str());
			} else {
				this->timer->cancel();
			}
			// newState string to index
			for (int i = 0; i < this->options.size(); i++) {
				if (newState == this->options.at(i)) {
					system(format(this->updateCommand, {std::to_string(i)}).c_str()); 
					break;
				}
			}

			this->timer = new Timer(2, [this]()->void {
				this->close();
			});
		}
		void close() {
			if (this->isOpen) {
				system(this->closeCommand.c_str());
				this->isOpen = false;
				this->timer = nullptr;
			}
		}
};

void printHelp() {
	std::cout << "I am not helping you lol. (TODO)" << std::endl;
}
void printErrorArgs() {
	std::cout << "worng arguments, try \"daemon help\" to get help." << std::endl;
}
void closeSnackbar();

int main(int argc, char* argv[]) {
	bool daemonRunning = false; //use shared memory
	Daemon daemon("sdf", "sfd", "ffds", {"test", "test2"}); //use shared memory
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
				daemon.close();
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
				daemon.update(argv[2]);
			} else {
				printErrorArgs();
			}
			break;
		default:
			printErrorArgs();
	}
}
