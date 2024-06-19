#include <iostream>
#include <cstring>
#include <string>

#include <unistd.h>
#include <sys/socket.h>
#include <sys/un.h>

int sendString(int fd, std::string str) {
	std::string length(std::to_string(str.length() + 1));
	send(fd, length.c_str(), length.length() + 1, 0);

	/*
	char lenBuffer[21] = {};
	recv(fd, lenBuffer, sizeof(lenBuffer), 0);
	unsigned long len = std::stoul(lenBuffer);
	*/

	send(fd, str.c_str(), str.length() + 1, 0);
	std::cout << "str: " << str << ", length: " << length << std::endl;

	/*
	if (len == str.length() + 1) {
		return 0;
	}

	return -1;
	*/
	return 0;
}

std::string recvString(int fd) {
	char lenBuffer[21] = {};
	recv(fd, lenBuffer, sizeof(lenBuffer), 0);

	// send(fd, lenBuffer, sizeof(lenBuffer), 0);

	char* buffer = new char[std::stoul(lenBuffer)];
	recv(fd, buffer, sizeof(buffer), 0);

	std::cout << "str: " << buffer << ", length: " << lenBuffer << std::endl;

	std::string rvalue(buffer);
	delete[] buffer;

	return rvalue;
}

void openDaemon() {
	int sockfd = socket(AF_UNIX, SOCK_STREAM, 0);

	unlink("/tmp/snackdaemon");

	sockaddr_un addr;
	strcpy(addr.sun_path, "/tmp/snackdaemon");
	addr.sun_family = AF_UNIX;
	if (bind(sockfd, (sockaddr *)&addr, sizeof(addr)) == -1) {
		perror("bind error");
	}

	if (listen(sockfd, 3) == -1) {
		perror("listen error");
	}

	while (true) {
		int clisockfd = accept(sockfd, nullptr, nullptr);
		
		char buffer[1024] = {};
		if (recv(clisockfd, buffer, sizeof(buffer), 0) == -1) {
			perror("recv error");
		}
		if (strcmp(buffer, "ping") == 0) {
			sendString(clisockfd, "pong");
			std::cout << "pinged!!" << std::endl;

		} else {
			std::cout << "message: " << buffer << std::endl;
		}
		close(clisockfd);
	}
	close(sockfd);
	unlink("/tmp/snackdaemon");
}


void sendMessage(std::string message) {
	int sockfd = socket(AF_UNIX, SOCK_STREAM, 0);

	sockaddr_un addr;
	strcpy(addr.sun_path, "/tmp/snackdaemon");
	addr.sun_family = AF_UNIX;
	if (connect(sockfd, (sockaddr *)&addr, sizeof(addr)) == -1) {
		perror("connect error");
	}
	
	send(sockfd, message.c_str(), message.length(), 0);
	std::cout << "message: " << message << std::endl;
	close(sockfd);
}

void pingDaemon() {
	int sockfd = socket(AF_UNIX, SOCK_STREAM, 0);

	sockaddr_un addr;
	strcpy(addr.sun_path, "/tmp/snackdaemon");
	addr.sun_family = AF_UNIX;
	if (connect(sockfd, (sockaddr *)&addr, sizeof(addr)) == -1) {
		perror("connect error");
	}
	
	std::string message("ping");
	send(sockfd, message.c_str(), message.length(), 0);

	std::string rec = recvString(sockfd);
	std::cout << rec << std::endl;
	close(sockfd);


}

int main(int argc, char* argv[]) {
	if (argc == 2 && strcmp(argv[1], "daemon") == 0) {
		std::cout << "openDaemon" << std::endl;
		openDaemon();
	} else if (argc == 2 && strcmp(argv[1], "send") == 0) {
		std::cout << "send" << std::endl;
		sendMessage("hello!!!");
	} else if (argc == 2 && strcmp(argv[1], "ping") == 0) {
		pingDaemon();
	} else if (argc == 3 && strcmp(argv[1], "send") == 0) {
		std::cout << "send" << std::endl;
		sendMessage(argv[2]);
	} else {
		std::cout << "daemon or send plz" << std::endl;
	}
}
