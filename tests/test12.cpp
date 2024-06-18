#include <asm-generic/socket.h>
#include <iostream>
#include <cstring>
#include <string>

#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>

#define PORT 8194

void openDaemon() {
	int sockfd = socket(AF_UNIX, SOCK_STREAM, 0);

	int int1 = 1;
	setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &int1, sizeof(int1));

	sockaddr_in addr;
	addr.sin_family = AF_UNIX;
	addr.sin_addr.s_addr = INADDR_ANY;
	addr.sin_port = htons(PORT);
	if (bind(sockfd, (sockaddr *)&addr, sizeof(addr)) == -1) {
		perror("bind error");
		return;
	}

	listen(sockfd, 3);

	int clisock = accept(sockfd, nullptr, nullptr);
	
	char buffer[1024] = {};
	recv(clisock, buffer, sizeof(buffer), 0);
	std::cout << "message: " << buffer << std::endl;

	close(sockfd);
	close(clisock);
}

void sendMessage(std::string message) {
	int sockfd = socket(AF_UNIX, SOCK_STREAM, 0);

	sockaddr_in addr;
	addr.sin_family = AF_UNIX;
	addr.sin_addr.s_addr = INADDR_ANY;
	addr.sin_port = htons(PORT);

	if (connect(sockfd, (sockaddr *)&addr, sizeof(addr)) == -1) {
		std::cout << "connect error, errno" << errno << std::endl;
		return;
	}
	
	send(sockfd, message.c_str(), message.length(), 0);
	std::cout << "message: " << message << std::endl;
	close(sockfd);
}

int main(int argc, char* argv[]) {
	if (argc == 2 && strcmp(argv[1], "daemon") == 0) {
		std::cout << "openDaemon" << std::endl;
		openDaemon();
	} else if (argc == 2 && strcmp(argv[1], "send") == 0) {
		std::cout << "send" << std::endl;
		sendMessage("hello!!!");
	} else {
		std::cout << "daemon or send plz" << std::endl;
	}
}
