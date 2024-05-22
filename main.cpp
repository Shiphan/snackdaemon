#include <cstddef>
#include <iostream>
#include <csignal>
#include <fstream>

#include <stdexcept>
#include <vector>
#include <map>
#include <string>
#include <strstream>
#include <thread>
#include <chrono>

#include <sys/mman.h>
#include <fcntl.h>

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

template<typename _Rep, typename _Period, typename Callable>
void timer(const std::chrono::duration<_Rep, _Period> &rtime, Callable callback, bool execNow = false) {
	pid_t pid = fork();
	if (pid == -1) {
		std::cout << "fork error" << std::endl;
		return;
	} else if (pid > 0) {
		return;
	}

	int fd = shm_open("timerId", O_CREAT | O_RDWR, 0666);
	ftruncate(fd, sizeof(int));
	void* sharedPtr = mmap(NULL, sizeof(int), PROT_WRITE, MAP_SHARED, fd, 0);

	(*(int *)sharedPtr)++;
	const int id = *(int *)sharedPtr;

	if (execNow) {
		callback();
	}

	std::this_thread::sleep_for(rtime);
	if (id == *(int *)sharedPtr){
		if (!execNow) {
			callback();
		}
		munmap(sharedPtr, sizeof(int));
		shm_unlink("timerId");
	} else {
		munmap(sharedPtr, sizeof(int));
	}
	exit(0);
}

void printHelp() {
	std::cout << "I am not helping you lol. (TODO)" << std::endl;
}
void printInvalidArgs() {
	std::cout << "invalid arguments, try \"daemon help\" to get help." << std::endl;
}
void printInvalidConfig() {
	std::cout << "invalid config file" << std::endl;
}

void update(std::string openCommand, std::string updateCommand, std::vector<std::string> options, std::string option) {
	int fd = shm_open("timerId", O_CREAT | O_RDWR, 0666);
	ftruncate(fd, sizeof(int));
	void* sharedPtr = mmap(NULL, sizeof(int), PROT_READ, MAP_SHARED, fd, 0);
	const int timerId = *(int *)sharedPtr;
	munmap(sharedPtr, sizeof(int));

	int optionIndex = -1;
	for (int i = 0; i < options.size(); i++) {
		if (option == options.at(i)) {
			optionIndex = i;
			break;
		}
	}

	if (optionIndex == -1) {
		std::cout << "no such option" << std::endl;
		return;
	}

	if (timerId == 0) {
		system(openCommand.c_str());
	}
	system(format(updateCommand, {std::to_string(optionIndex)}).c_str());
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

int main(int argc, char* argv[]) {
	auto [keyToValue, keyToValuelist] = loadConfig("/home/shiphan/.config/snackdaemon/snackdaemon.conf");

	if (!validConfig(keyToValue, keyToValuelist)) {
		printInvalidConfig();
		return 0;
	}
	
	std::chrono::duration timeout = std::chrono::milliseconds(std::stoi(keyToValue.at("timeout")));
	std::string openCommand = keyToValue.at("openCommand");
	std::string updateCommand = keyToValue.at("updateCommand");
	std::string closeCommand = keyToValue.at("closeCommand");
	std::vector<std::string> options = keyToValuelist.at("options");

	if (argc == 3 && std::string(argv[1]) == std::string("update")) {
		update(openCommand, updateCommand, options, argv[2]);
		timer(timeout, [closeCommand](){system(closeCommand.c_str());});
	} else if (argc == 2 && std::string(argv[1]) == std::string("close")) {
		timer(timeout, [closeCommand](){system(closeCommand.c_str());}, true);
	} else if (argc == 2 && std::string(argv[1]) == std::string("help")) {
		printHelp();
	} else {
		printInvalidArgs();
	}
	return 0;
}
