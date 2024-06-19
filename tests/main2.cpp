#include <csignal>

#include <cstddef>
#include <iostream>
#include <fstream>
#include <iomanip>
#include <vector>
#include <map>
#include <cstring>
#include <string>
#include <sstream>
#include <thread>
#include <chrono>

#include <unistd.h>
#include <sys/socket.h>
#include <sys/un.h>

#define SERVER_PATH "/tmp/snackdaemon"

class Timer{
private:
	bool canceled;
	Timer** timer;

public:
	template<typename _Rep, typename _Period, typename Callable>
	Timer(const std::chrono::duration<_Rep, _Period> &rtime, Callable callback, Timer** timer) {
		this->canceled = false;
		this->timer = timer;
		std::thread timerThread([this, rtime, callback]()-> void {
			std::this_thread::sleep_for(rtime);
			if (!this->canceled){
				callback();
				*(this->timer) = nullptr;
			}
			delete this;
		});
		timerThread.detach();
	}
	~Timer() {
	}
	void cancel() {
		this->canceled = true;
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
	ping,
	update,
	closeNow,
	killDaemon
};

int sendString(int fd, std::string str) {
	std::string length(std::to_string(str.length() + 1));
	send(fd, length.c_str(), length.length() + 1, 0);

	char lenBuffer[21] = {};
	recv(fd, lenBuffer, sizeof(lenBuffer), 0);
	unsigned long len = std::stoul(lenBuffer);

	send(fd, str.c_str(), str.length() + 1, 0);

	if (len == str.length() + 1) {
		return 0;
	}

	return -1;
}

std::string recvString(int fd) {
	char lenBuffer[21] = {};
	recv(fd, lenBuffer, sizeof(lenBuffer), 0);

	send(fd, lenBuffer, sizeof(lenBuffer), 0);

	char* buffer = new char[std::stoul(lenBuffer)];
	recv(fd, buffer, sizeof(buffer), 0);

	std::string rvalue(buffer);
	delete[] buffer;

	return rvalue;
}


std::tuple<std::map<std::string, std::string>, std::map<std::string, std::vector<std::string>>> loadConfig(std::string filePath) {
	std::map<std::string, std::string> keyToValue;
	std::map<std::string, std::vector<std::string>> keyToValuelist;

	std::string line;
	std::ifstream config(filePath);
	while (getline(config, line)) {
		size_t equalIndex = line.find('=');
		if (equalIndex == std::string::npos) {
			continue;
		}
		
		size_t keyPos = line.find_first_not_of(" \t\n");
		if (keyPos == equalIndex) {
			continue;
		}

		size_t valuePos = line.find_first_not_of(" \t\n", equalIndex + 1);
		if (valuePos == std::string::npos) {
			continue;
		}

		size_t keyEnd = line.find_last_not_of(" \t\n", equalIndex - 1);
		std::string key = line.substr(keyPos, keyEnd - keyPos + 1);
		
		std::string value = line.substr(valuePos);

		if (value.size() > 0 && value.at(0) == '[') {
			std::vector<std::string> valueList = {};

			value = value.substr(1);
			size_t valueEnd = value.find_last_not_of("] \t\n");
			if (valueEnd != std::string::npos) {
				valuePos = value.find_first_not_of(" \t\n");
				valueList.push_back(value.substr(valuePos, valueEnd - valuePos + 1));
			}
			while (value.find_first_of("]") == std::string::npos) {
				getline(config, value);
				size_t valueEnd = value.find_last_not_of("] \t\n");
				if (valueEnd == std::string::npos) {
					continue;
				}
				valuePos = value.find_first_not_of(" \t\n");
				valueList.push_back(value.substr(valuePos, valueEnd - valuePos + 1));
			}

			keyToValuelist[key] = valueList;
		} else {
			keyToValue[key] = value;
		}
	}
	config.close();
	
	return {keyToValue, keyToValuelist};
}

bool validConfig(std::map<std::string, std::string> keyToValue, std::map<std::string, std::vector<std::string>> keyToValuelist) {
	std::vector<std::string> keys = {"timeout", "openCommand", "updateCommand", "closeCommand", "options"};
	for (int i = 0; i < keys.size(); i++) {
		if (keyToValue.count(keys.at(i)) == 0 && keyToValuelist.count(keys.at(i)) == 0) {
			return false;
		}
	}

	try {
		std::stoi(keyToValue.at("timeout"));
	} catch (std::invalid_argument) {
		return false;
	}

	return true;
}

void openDaemon() {
	if (getenv("HOME") == NULL) {
		std::cout << "can not get your home directory." << std::endl;
		return;
	}
	std::string homedir = getenv("HOME");

	auto [keyToValue, keyToValuelist] = loadConfig(homedir + "/.config/snackdaemon/snackdaemon.conf");

	if (!validConfig(keyToValue, keyToValuelist)) {
		printInvalidConfig();
		return;
	}
	
	std::chrono::duration timeout = std::chrono::milliseconds(std::stoi(keyToValue.at("timeout")));
	std::string openCommand = keyToValue.at("openCommand");
	std::string updateCommand = keyToValue.at("updateCommand");
	std::string closeCommand = keyToValue.at("closeCommand");
	std::vector<std::string> options = keyToValuelist.at("options");

	int sockfd = socket(AF_UNIX, SOCK_STREAM, 0);

	unlink(SERVER_PATH);

	sockaddr_un addr;
	strcpy(addr.sun_path, SERVER_PATH);
	addr.sun_family = AF_UNIX;
	if (bind(sockfd, (sockaddr *)&addr, sizeof(addr)) == -1) {
		perror("bind error");
	}

	if (listen(sockfd, 10) == -1) {
		perror("listen error");
	}

	Timer* timer;
	bool running = true;
	while (running) {
		int clisockfd = accept(sockfd, nullptr, nullptr);
		
		char messageBuffer[11] = {};
		if (recv(clisockfd, messageBuffer, sizeof(messageBuffer), 0) == -1) {
			perror("recv error");
		}
		
		std::cout << "message: "<< messageBuffer << std::endl;
		switch ((message)std::stoi(messageBuffer)) {
			case ping: {
				std::cout << "ping" << std::endl;
				sendString(clisockfd, "pong\n");
				break;
			}
			case killDaemon: {
				std::cout << "kill" << std::endl;
				sendString(clisockfd, "ok\n");
				running = false;
				break;
			}
			case closeNow: {
				std::cout << "close" << std::endl;
				sendString(clisockfd, "");
				if (timer != nullptr) {
					timer->cancel();
					system(closeCommand.c_str());
					timer = nullptr;
				}
				break;
			}
			case update: {
				std::cout << "update: ";

				std::string mess = std::to_string(update);
				send(sockfd, mess.c_str(), mess.length() + 1, 0);

				std::string option = recvString(clisockfd);

				std::cout << option;

				int optionIndex = -1;
				for (int i = 0; i < options.size(); i++) {
					if (option == options.at(i)) {
						optionIndex = i;
						break;
					}
				}

				if (optionIndex == -1) {
					sendString(clisockfd, "no such option\n");
					std::cout << " (no such option)" << std::endl;
				} else {
					sendString(clisockfd, "");
					std::cout << std::endl;

					if (timer == nullptr) {
						system(openCommand.c_str());
					}
					system(format(updateCommand, {std::to_string(optionIndex)}).c_str());
					timer = new Timer(timeout, [closeCommand](){system(closeCommand.c_str());}, &timer);
				}
				break;
			}
			default: {
				std::cout << "unknown message" << std::endl;
			}
		}
	
		close(clisockfd);
	}

	close(sockfd);
	unlink(SERVER_PATH);
}

void sendMessage(message message) {
	int sockfd = socket(AF_UNIX, SOCK_STREAM, 0);

	sockaddr_un addr;
	strcpy(addr.sun_path, SERVER_PATH);
	addr.sun_family = AF_UNIX;
	if (connect(sockfd, (sockaddr *)&addr, sizeof(addr)) == -1) {
		perror("connect error");
		return;
	}
	
	std::string mess = std::to_string(message);
	send(sockfd, mess.c_str(), mess.length() + 1, 0);
	
	std::string respond = recvString(sockfd);
	std::cout << respond;

	close(sockfd);
}

void updateSnackbar(std::string option) {
	int sockfd = socket(AF_UNIX, SOCK_STREAM, 0);

	sockaddr_un addr;
	strcpy(addr.sun_path, SERVER_PATH);
	addr.sun_family = AF_UNIX;
	if (connect(sockfd, (sockaddr *)&addr, sizeof(addr)) == -1) {
		perror("connect error");
		return;
	}
	
	std::string mess = std::to_string(update);
	send(sockfd, mess.c_str(), mess.length() + 1, 0);

	char messBuffer[21] = {};
	recv(sockfd, messBuffer, sizeof(messBuffer), 0);

	sendString(sockfd, option);
	
	std::string respond = recvString(sockfd);
	std::cout << respond;

	close(sockfd);
}

int main(int argc, char* argv[]) {
	if (argc == 1) {
		printHelp();
	} else if (argc == 2) {
		if (strcmp(argv[1], "daemon") == 0) {
			openDaemon();
		} else if (strcmp(argv[1], "kill") == 0) {
			sendMessage(killDaemon);
		} else if (strcmp(argv[1], "ping") == 0) {
			sendMessage(ping);
		} else if (strcmp(argv[1], "close") == 0) {
			sendMessage(closeNow);
		} else if (strcmp(argv[1], "help") == 0) {
			printHelp();
		} else {
			printInvalidArgs();
		}
	} else if (argc == 3 && strcmp(argv[1], "update") == 0) {
		updateSnackbar(std::string(argv[2]));
	} else {
		printInvalidArgs();
	}

	return 0;
}
