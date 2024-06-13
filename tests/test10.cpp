#include <csignal>
#include <exception>
#include <iostream>

#include <stdexcept>
#include <string>

#include <sys/mman.h>
#include <fcntl.h>

template<typename type>
class SharedMemory {
protected:
	std::string name;
	bool closed;
	type* sharedPtr;
	
public:
	SharedMemory(std::string name, int oflag = O_CREAT | O_RDWR, mode_t mode = 0666, int prot = PROT_WRITE, int flags = MAP_SHARED) {
		this->closed = false;
		this->name = name;

		int fd = shm_open(name.c_str(), oflag, mode);
		ftruncate(fd, sizeof(type));
		this->sharedPtr = (type *)mmap(NULL, sizeof(type), prot, flags, fd, 0);
	}
	~SharedMemory() {
		this->close();
	}
	void close() {
		if (this->closed) {
			return;
		}

		munmap(this->sharedPtr, sizeof(type));
		this->closed = true;
	}
	void unlink() {
		if (this->closed) {
			return;
		}

		this->close();
		shm_unlink(name.c_str());
	}
	type getData() {
		if (this->closed) {
			throw std::runtime_error("This shared memory object has been closed, you cannot access its data.");
		}

		return *sharedPtr;
	}
	void setData(type data) {
		if (this->closed) {
			throw std::runtime_error("This shared memory object has been closed, you cannot access its data.");
		}

		*sharedPtr = data;
	}
};

int main(int argc, char* argv[]) {

	
	SharedMemory<int> shm("testid");
	shm.setData(shm.getData() + 10);
	std::cout << "testid: " << shm.getData() << std::endl;
	shm.unlink();
	shm.setData(shm.getData() + 10);
	std::cout << "testid: " << shm.getData() << std::endl;

	SharedMemory<int> shm2("testid");
	std::cout << "testid2: " << shm2.getData() << std::endl;
	shm2.setData(100);
	std::cout << "testid2: " << shm2.getData() << std::endl;

	std::cout << "testid: " << shm.getData() << std::endl;

}
