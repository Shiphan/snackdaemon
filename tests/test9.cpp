#include <iostream>
#include <csignal>
#include <sys/mman.h>
#include <fcntl.h>

int main() {
	std::cout << "home" << std::endl;
	shm_unlink("isSnackbarOpen");
	shm_unlink("timerId");
	std::cout << "home" << std::endl;

	std::cout << std::string(getenv("HOME")) << std::endl;
	std::cout << "home" << std::endl;

	int fd = shm_open("isSnackbarOpen", O_CREAT | O_RDWR, 0666);
	std::cout << fd << std::endl;
	ftruncate(fd, sizeof(bool));
	void* sharedPtr = mmap(NULL, sizeof(bool), PROT_WRITE, MAP_SHARED, fd, 0);

	if (!(*(bool*)sharedPtr)) {
		std::cout << true << std::endl;
	} else {
		std::cout << false << std::endl;
	}
	(*(bool*)sharedPtr) = !(*(bool*)sharedPtr);
	const bool isOpen = *(bool *)sharedPtr;
	std::cout << isOpen << std::endl;

	munmap(sharedPtr, sizeof(bool));
	shm_unlink("isSnackbarOpen");


	fd = shm_open("timerId", O_CREAT | O_RDWR, 0666);
	std::cout << fd << std::endl;
	ftruncate(fd, sizeof(int));
	sharedPtr = mmap(NULL, sizeof(int), PROT_WRITE, MAP_SHARED, fd, 0);

	(*(int *)sharedPtr) = 99;
	const int timerId = *(int *)sharedPtr;
	std::cout << timerId << std::endl;

	munmap(sharedPtr, sizeof(int));
	shm_unlink("timerId");
	std::cout << "home" << std::endl;
}
