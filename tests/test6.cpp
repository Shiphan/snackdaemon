#include <chrono>
#include <iostream>
#include <thread>
#include <string>
#include <sys/mman.h>
#include <fcntl.h>

template<typename _Rep, typename _Period, typename Callable>
void timer(const std::chrono::duration<_Rep, _Period> &rtime, Callable callback, bool pseudo = false) {
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

	if (pseudo) {
		callback();
		munmap(sharedPtr, sizeof(int));
		shm_unlink("timerId");
		return;
	}

	std::this_thread::sleep_for(rtime);
	if (id == *(int *)sharedPtr){
		callback();
		munmap(sharedPtr, sizeof(int));
		shm_unlink("timerId");
	} else {
		munmap(sharedPtr, sizeof(int));
	}
	return;
}

int main(int argc, char* argv[]) {
	void(* notify)(void) = []()->void {
		system("notify-send TEST");
	};
	if (argc == 2 && std::string(argv[1]) == std::string("run")) {
		timer(std::chrono::seconds(2), notify);
	} else if (argc == 2 && std::string(argv[1]) == std::string("force")) {
		timer(std::chrono::seconds(2), notify, true);
	} else {
		std::cout << "invalid args" << std::endl;
	}
}
