#include "ircserv.hpp"

int live = true;

void	handle_sigint(int signum)
{
	(void) signum;
	live = false;
}

int main(int argc, char *argv[]) {
	signal(SIGQUIT, SIG_IGN);
	signal(SIGINT, &handle_sigint);
	if (argc != 3 || atoi(argv[1]) < 49152 || atoi(argv[1]) > 65535) {
		std::cerr << "Usage: ./ircserv <port> (49152-65535) <password>" << std::endl;
		return 1;
	}
	int port = atoi(argv[1]);
	std::string password = argv[2];

	IRCServer server(port, password);
	server.start();

	return 0;
}

