#include <iostream>
#include <stdexcept>
#include <string>
#include <map>

int main() {
	std::map<std::string, std::string> ma = {{"b", "sdf"}};
	std::cout << ma.at("a") << std::endl;
	std::string st;
	std::cin >> st;
	int in;
	try {
		in = std::stoi(st);
	} catch(std::invalid_argument) {
		in = -1;
	}
	std::cout << in << std::endl;
}
