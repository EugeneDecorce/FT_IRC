#include "../ircserv.hpp"

bool IRCServer::is_nickname_taken(const std::string &nickname) {
	for (std::map<int, std::string>::iterator it = nicknames.begin(); it != nicknames.end(); ++it) {
		if (it->second == nickname) {
			return true;
		}
	}
	return false;
}

void IRCServer::accept_new_client() {
	struct sockaddr_in client_addr;
	socklen_t client_len = sizeof(client_addr);
	int new_client = accept(server_socket, (struct sockaddr *)&client_addr, &client_len);
	if (new_client < 0) {
		std::cerr << "Error: Cannot accept new client" << std::endl;
		return;
	}

	set_non_blocking(new_client);

	struct pollfd client_pollfd;
	client_pollfd.fd = new_client;
	client_pollfd.events = POLLIN;
	client_pollfd.revents = 0;
	fds.push_back(client_pollfd);

	clients[new_client] = "";
	usernames[new_client] = "";
	nicknames[new_client] = "";

	std::cout << "New client connected: " << inet_ntoa(client_addr.sin_addr) << ", client_socket: " << new_client << std::endl;
}

void IRCServer::handle_client(int client_socket) {
	char buffer[BUFFER_SIZE];
	int bytes_read = recv(client_socket, buffer, BUFFER_SIZE - 1, 0);  // -1 to leave space for null terminator
	if (bytes_read <= 0) {
		remove_client(client_socket);
		return;
	}

	buffer[bytes_read] = '\0';  // Null-terminate the received data
	clients[client_socket] += buffer;  // Accumulate data in the client's buffer

	size_t pos;
	while ((pos = clients[client_socket].find("\n")) != std::string::npos) {
		std::string command = clients[client_socket].substr(0, pos);
		clients[client_socket].erase(0, pos + 1);  // Remove the processed command
		process_command(client_socket, command);  // Process the full command
	}
} 

void IRCServer::remove_client(int client_socket) {
	close(client_socket);
	clients.erase(client_socket);
	nicknames.erase(client_socket);
	authenticated_clients[client_socket] = false;

	for (std::map<std::string, std::set<int> >::iterator it = channels.begin(); it != channels.end(); ++it) {
		it->second.erase(client_socket);
	}

	for (size_t i = 0; i < fds.size(); i++) {
		if (fds[i].fd == client_socket) {
			fds.erase(fds.begin() + i);
			break;
		}
	}

	std::cout << "Client disconnected: " << client_socket << std::endl;
}

void IRCServer::send_to_client(int client_socket, const std::string &message) {
	send(client_socket, message.c_str(), message.length(), 0);
}