#include <cstring>
#include <iostream>
#include <string>

#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

int main(int argc, char* argv[]) {
using namespace std;

	if (argc == 2 && std::string(argv[1]) == std::string("open")) {
		int fd = shm_open("shared", O_CREAT | O_RDWR, 0666);
		std::cout << "fd: " << fd << std::endl;
		ftruncate(fd, sizeof(string));
		void* sharedPtr = mmap(NULL, sizeof(string), PROT_WRITE, MAP_SHARED, fd, 0);
		
		string* str = new string("test");
		memcpy(sharedPtr, str, sizeof(string));
		delete str;

		std::cout << "str: " << *(string *)sharedPtr << std::endl;


	} else if (argc == 2 && std::string(argv[1]) == std::string("run")) {
		int fd = shm_open("shared", O_CREAT | O_RDWR, 0666);
		std::cout << "fd: " << fd << std::endl;
		ftruncate(fd, sizeof(string));
		void* sharedPtr = mmap(NULL, sizeof(string), PROT_WRITE | PROT_EXEC, MAP_SHARED, fd, 0);

		std::cout << "str: " << *(string *)sharedPtr << std::endl;
		((string *)sharedPtr)->append("1");
		std::cout << "str: " << *(string *)sharedPtr << std::endl;

	} else if (argc == 2 && std::string(argv[1]) == std::string("kill")) {
		shm_unlink("shared");
	} else {
		std::cout << "invalid args" << std::endl;
	}
}
