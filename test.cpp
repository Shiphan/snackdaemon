#include <iostream>
#include <unistd.h>
#include <format>

int main(int argc, char* argv[]) {
	using namespace std;
	pid_t pid = fork();
	
	if (pid == -1) {
		cout << "fork error" << endl;
	} else if (pid > 0) { // main
		cout << "out from main" << endl;
		system("notify-send \"out from main\"");
		if (argc > 1) {
			string newstr = format("notify-send \"out from main + {}\"", argv[1]);
			system(newstr.c_str());
		}
	} else if (pid == 0) {
		sleep(2);
		cout << "out from child" << endl;
		system("notify-send \"out from child\"");
	}
		
}
