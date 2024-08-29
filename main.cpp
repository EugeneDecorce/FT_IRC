#include "ircserv.hpp"

int main(int argc, char *argv[]) {
	if (argc != 3 || atoi(argv[1]) < 49152 || atoi(argv[1]) > 65535) {
		std::cerr << "Usage: ./ircserv <port> (49152-65535) <password>" << std::endl;
		return EXIT_FAILURE;
	}

	int port = atoi(argv[1]);
	std::string password = argv[2];

	IRCServer server(port, password);
	server.start();

	return 0;
}

