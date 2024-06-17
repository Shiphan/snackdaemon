#include <csignal>
#include <stdexcept>

#include <iostream>
#include <iomanip>
#include <vector>
#include <cstring>
#include <string>
#include <sstream>

#include <sys/mman.h>
#include <fcntl.h>

template<typename type>
class SharedMemory {
protected:
	std::string name;
	bool closed;
	type* sharedPtr;
	
public:
	SharedMemory(std::string name, int oflag = O_CREAT | O_RDWR, mode_t mode = 0666, int prot = PROT_WRITE, int flags = MAP_SHARED) {
		this->closed = false;
		this->name = name;

		int fd = shm_open(name.c_str(), oflag, mode);
		ftruncate(fd, sizeof(type));
		this->sharedPtr = (type *)mmap(NULL, sizeof(type), prot, flags, fd, 0);
	}
	~SharedMemory() {
		this->close();
	}
	void close() {
		if (this->closed) {
			return;
		}

		munmap(this->sharedPtr, sizeof(type));
		this->closed = true;
	}
	void unlink() {
		if (this->closed) {
			throw std::runtime_error("This shared memory object has been closed, you cannot access its data.");
		}

		this->close();
		shm_unlink(name.c_str());
	}
	std::string getName() {
		return this->name;
	}
	type getData() {
		if (this->closed) {
			throw std::runtime_error("This shared memory object has been closed, you cannot access its data.");
		}

		return *sharedPtr;
	}
	void setData(type data) {
		if (this->closed) {
			throw std::runtime_error("This shared memory object has been closed, you cannot access its data.");
		}

		*sharedPtr = data;
	}
};

std::string format(std::string format, std::vector<std::string> values) {
	std::stringstream strstream;
	int valuesIndex = 0;
	size_t pos = 0;
	size_t index = format.find('{');
	while (index != std::string::npos && valuesIndex < values.size()) {
		if (index + 1 < format.size() && format.at(index + 1) == '}') {
			if (index == 0 || format.at(index - 1) != '\\') {
				strstream << format.substr(pos, index - pos) << values.at(valuesIndex);
				valuesIndex++;
			} else {
				strstream << format.substr(pos, index - pos - 1) << "{}";
			}
			pos = index + 2;
		} else {
			strstream << format.substr(pos, index - pos + 1);
			pos = index + 1;
		}
		index = format.find('{', pos);
	}
	if (pos < format.size()) {
		strstream << format.substr(pos);
	}
	return strstream.str();
}

void printHelp() {
	std::cout << "usage: snackdaemon <command> [<args>]\n"
	<< "commands:\n"
	<< std::left
	<< std::setw(20) << "    help" << "Print help\n"
	<< std::setw(20) << "    update <arg>" << "Update with <arg>'s index in \"options\" in config file\n"
	<< std::setw(20) << "    close" << "Trigger the \"closeCommand\" in config file and end timer\n"
	<< "\n"
	<< "Visit 'https://github.com/Shiphan/snackdaemon' for more information or bug report.\n";
}
void printInvalidArgs() {
	std::cout << "invalid arguments, try `snackdaemon help` to get help." << std::endl;
}
void printInvalidConfig() {
	std::cout << "invalid config file" << std::endl;
}

enum message {
	error,
	respond,
	ping,
	update,
	closeNow
};

void updateSnackbar(std::string arg) {
}

void closeSnackbar() {
}

void pingDaemon() {
	SharedMemory<std::string> shm("ipcBuffer");
	shm.setData(std::to_string(ping));

	// ping daemon to read this
	kill(0, SIGUSR1);

	__sighandler_t sigusr1Handler;
	signal(SIGUSR1, sigusr1Handler);
	
	sigset_t set;
	sigemptyset(&set);
	sigaddset(&set, SIGUSR1);
	siginfo_t info;
	struct timespec timeout{10, 0};

	int signum = sigtimedwait(&set, &info, &timeout);

	if (signum == -1 && errno == EAGAIN) {
		std::cout << "timed out and daemon did not respond" << std::endl;
	} else if (signum == SIGUSR1) {
		std::cout << shm.getData();
	}
	shm.unlink();
}

void openDaemon() {

}

void killDaemon() {

}

int main(int argc, char* argv[]) {
	if (argc == 1) {
		printHelp();
	} else if (argc == 2) {
		if (strcmp(argv[1], "daemon")) {
			openDaemon();
		} else if (strcmp(argv[1], "kill")) {
			killDaemon();
		} else if (strcmp(argv[1], "ping")) {
			pingDaemon();
		} else if (strcmp(argv[1], "close")) {
			closeSnackbar();
		} else if (strcmp(argv[1], "help")) {
			printHelp();
		} else {
			printInvalidArgs();
		}
	} else if (argc == 3 && strcmp(argv[1], "update")) {
		updateSnackbar(std::string(argv[2]));
	} else {
		printInvalidArgs();
	}

	return 0;
}
