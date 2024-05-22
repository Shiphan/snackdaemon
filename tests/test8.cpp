#include <iostream>
#include <chrono>
#include <map>
#include <vector>
#include <string>
#include <fstream>

std::tuple<std::map<std::string, std::string>, std::map<std::string, std::vector<std::string>>> loadConfig(std::string filePath) {
	std::map<std::string, std::string> keyToValue;
	std::map<std::string, std::vector<std::string>> keyToValuelist;

	std::string line;
	std::ifstream config("./snackdaemon.conf");
	while (getline(config, line)) {
		std::cout << "OMG!!!" << std::endl;

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
	const std::string homedir = std::getenv("HOME");
	auto [keyToValue, keyToValuelist] = loadConfig(homedir + "/.config/snackdaemon/snackdaemon.conf");

	if (!validConfig(keyToValue, keyToValuelist)) {
		std::cout << "invalid config" << std::endl;
		return 0;
	}
	
	std::chrono::duration time = std::chrono::milliseconds(std::stoi(keyToValue.at("timeout")));
	std::string openCommand = keyToValue.at("openCommand");
	std::string updateCommand = keyToValue.at("updateCommand");
	std::string closeCommand = keyToValue.at("closeCommand");
	std::vector<std::string> options = keyToValuelist.at("options");

	for (int i = 0; i < options.size(); i++) {
		std::cout << options.at(i) << std::endl;
	}
	while (true) {
		std::string key;
		std::cin >> key;
		std::cout << keyToValue.at(key) << std::endl;
	}

	return 0;
}
