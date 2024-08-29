/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   ircserv.cpp                                        :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: edecorce <edecorce@student.42.fr>          +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2024/08/24 14:59:42 by edecorce          #+#    #+#             */
/*   Updated: 2024/08/29 20:01:15 by edecorce         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "ircserv.hpp"

IRCServer::IRCServer(int port, const std::string &password)
	: password(password) {
	server_socket = socket(AF_INET, SOCK_STREAM, 0);
	if (server_socket < 0) {
		std::cerr << "Error: Cannot create socket" << std::endl;
		exit(EXIT_FAILURE);
	}

	struct sockaddr_in server_addr;
	server_addr.sin_family = AF_INET;
	server_addr.sin_addr.s_addr = INADDR_ANY;
	server_addr.sin_port = htons(port);

	if (bind(server_socket, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
		std::cerr << "Error: Cannot bind socket" << std::endl;
		exit(EXIT_FAILURE);
	}

	if (listen(server_socket, MAX_CLIENTS) < 0) {
		std::cerr << "Error: Cannot listen on socket" << std::endl;
		exit(EXIT_FAILURE);
	}

	set_non_blocking(server_socket);

	struct pollfd server_pollfd;
	server_pollfd.fd = server_socket;
	server_pollfd.events = POLLIN;
	server_pollfd.revents = 0;
	fds.push_back(server_pollfd);
}

IRCServer::~IRCServer() {
	close(server_socket);
}

void IRCServer::set_non_blocking(int socket) {
	if (fcntl(socket, F_SETFL, O_NONBLOCK) < 0) {
		std::cerr << "Error: Cannot set socket to non-blocking" << std::endl;
		exit(EXIT_FAILURE);
	}
}

void IRCServer::start() {
	
	while (live) {
		int poll_count = poll(&fds[0], fds.size(), -1);
		if (!live)
			break ;
		if (poll_count < 0) {
			std::cerr << "Error: Polling failed" << std::endl;
			exit(EXIT_FAILURE);
		}
		for (size_t i = 0; i < fds.size(); i++) {
			if (fds[i].revents & POLLIN) {
				if (fds[i].fd == server_socket) {
					accept_new_client();
				} else {
					handle_client(fds[i].fd);
				}
			}
		}
	}
}
