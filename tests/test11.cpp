#include <csignal>
#include <iostream>

int main() {
	__sighandler_t sigusr1Handler;
	signal(SIGUSR1, sigusr1Handler);

	sigset_t set;
	sigemptyset(&set);
	sigaddset(&set, SIGUSR1);
	//??????
	siginfo_t info;

	struct timespec timeout{10, 0};
	int signum = sigtimedwait(&set, &info, &timeout);
	if (signum == -1 && errno == EAGAIN) {
		std::cout << "timed out and daemon did not respond" << std::endl;
		kill(0, SIGINT);
	} else if (signum == SIGUSR1) {
		std::cout << "SIGUSR1: " << SIGUSR1 << ", signum: " << signum << std::endl;
	}
	return 0;
}
