#include <cstring>
#include <iostream>
#include <string>

#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

class Lab {
private:
	std::string str;

public:
	Lab(std::string str) :str(str) {

	}
	void print(int argc, char* argv[]) {
		std::cout << this->str << std::endl;
		std::string notify = "notify-send \"";
		for (int i = 0; i < argc; i++) {
			notify.append(argv[i]);
		}
		notify.append("\"");
		system(notify.c_str());
	}
	std::string getStr() {
		return this->str;
	}

};

int main(int argc, char* argv[]) {
	if (argc == 2 && std::string(argv[1]) == std::string("daemon")) {
		int fd = shm_open("shared", O_CREAT | O_RDWR, 0666);
		std::cout << "fd: " << fd << std::endl;
		ftruncate(fd, sizeof(Lab));
		void* sharedPtr = mmap(NULL, sizeof(Lab), PROT_WRITE, MAP_SHARED, fd, 0);
		

		Lab* lab = new Lab("test");
		memcpy(sharedPtr, lab, sizeof(Lab));
		std::cout << "Lab's str: " << ((Lab *)sharedPtr)->getStr() << std::endl;


	} else if (argc == 2 && std::string(argv[1]) == std::string("run")) {
		int fd = shm_open("shared", O_CREAT | O_RDWR, 0666);
		ftruncate(fd, sizeof(Lab));
		void* sharedPtr = mmap(NULL, sizeof(Lab), PROT_READ | PROT_WRITE | PROT_EXEC, MAP_SHARED, fd, 0);

		std::cout << "Lab's str: " << ((Lab *)sharedPtr)->getStr() << std::endl;
		// Lab* lab = (Lab *)sharedPtr;
		// lab->print(argc, argv);

	} else if (argc == 2 && std::string(argv[1]) == std::string("kill")) {
		shm_unlink("shared");
	} else {
		std::cout << "invalid args" << std::endl;
	}
}
