#include <iostream>
#include <string>
#include <strstream>
#include <vector>

std::string format(std::string format, std::vector<std::string> values) {
	std::ostrstream strstream;
	int valuesIndex = 0;
	for (int i = 0; i < format.length(); i++) {
		if (format.at(i) == '\\' && format.length() > i+1 && format.at(i+1) == '{') {
			i++;
			strstream << "{";
			i++;
			strstream << format[i];
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

int main() {
	std::string str = "abc {} \\def \\{} {}";
	std::string res = format(str.c_str(), {"xyz", "aaaa"});
	std::cout << res << std::endl;
}
