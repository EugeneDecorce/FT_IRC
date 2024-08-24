/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   ircserv.cpp                                        :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: edecorce <edecorce@student.42.fr>          +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2024/08/24 14:59:42 by edecorce          #+#    #+#             */
/*   Updated: 2024/08/24 14:59:48 by edecorce         ###   ########.fr       */
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
	fds.push_back(server_pollfd);
}

IRCServer::~IRCServer() {
	close(server_socket);
}

void IRCServer::set_non_blocking(int socket) {
	int flags = fcntl(socket, F_GETFL, 0);
	if (flags < 0) {
		std::cerr << "Error: Cannot get socket flags" << std::endl;
		exit(EXIT_FAILURE);
	}
	if (fcntl(socket, F_SETFL, flags | O_NONBLOCK) < 0) {
		std::cerr << "Error: Cannot set socket to non-blocking" << std::endl;
		exit(EXIT_FAILURE);
	}
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
	fds.push_back(client_pollfd);

	clients[new_client] = "";
	nicknames[new_client] = "";

	std::cout << "New client connected: " << inet_ntoa(client_addr.sin_addr) << std::endl;
}

void IRCServer::handle_client(int client_socket) {
	char buffer[BUFFER_SIZE];
	int bytes_read = recv(client_socket, buffer, BUFFER_SIZE, 0);
	if (bytes_read <= 0) {
		remove_client(client_socket);
		return;
	}

	buffer[bytes_read] = '\0';
	std::string message(buffer);

	std::istringstream iss(message);
	std::string command;
	while (getline(iss, command)) {
		process_command(client_socket, command);
	}
}

void IRCServer::remove_client(int client_socket) {
	close(client_socket);
	clients.erase(client_socket);
	nicknames.erase(client_socket);

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

void IRCServer::send_to_channel(const std::string &channel, const std::string &message, int sender_socket) {
	if (channels.find(channel) == channels.end())
		return;

	for (std::set<int>::iterator it = channels[channel].begin(); it != channels[channel].end(); ++it) {
		if (*it != sender_socket) {
			send_to_client(*it, message);
		}
	}
}

bool IRCServer::is_nickname_taken(const std::string &nickname) {
	for (std::map<int, std::string>::iterator it = nicknames.begin(); it != nicknames.end(); ++it) {
		if (it->second == nickname) {
			return true;
		}
	}
	return false;
}

void IRCServer::join_channel(int client_socket, const std::string &channel_name) {
	ChannelMode &chan_mode = channel_modes[channel_name];

	// Check for invite-only mode
	if (chan_mode.invite_only) {
		send_to_client(client_socket, "Channel is invite-only\n");
		return;
	}
	// Check for channel key (password)
	if (!chan_mode.key.empty()) {
		send_to_client(client_socket, "Enter channel key: ");
		char buffer[BUFFER_SIZE];
		int bytes_read = recv(client_socket, buffer, BUFFER_SIZE, 0);
		buffer[bytes_read] = '\0';
		std::string entered_key(buffer);
		if (entered_key != chan_mode.key) {
			send_to_client(client_socket, "Incorrect channel key\n");
			return;
		}
	}
	// Check for user limit
	if (chan_mode.user_limit > 0 && channels[channel_name].size() >= (size_t)chan_mode.user_limit) {
		send_to_client(client_socket, "Channel is full\n");
		return;
	}
	// Add the client to the channel
	channels[channel_name].insert(client_socket);
	send_to_client(client_socket, "Joined channel: " + channel_name + "\n");
	send_to_channel(channel_name, nicknames[client_socket] + " has joined the channel\n", client_socket);
}


void IRCServer::handle_privmsg(int client_socket, const std::string &target, const std::string &message) {
	if (target[0] == '#') {
		send_to_channel(target, nicknames[client_socket] + ": " + message + "\n", client_socket);
	} else {
		for (std::map<int, std::string>::iterator it = nicknames.begin(); it != nicknames.end(); ++it) {
			if (it->second == target) {
				send_to_client(it->first, nicknames[client_socket] + " (private): " + message + "\n");
				return;
			}
		}
		send_to_client(client_socket, "No such user: " + target + "\n");
	}
}

void IRCServer::process_command(int client_socket, const std::string &command) {
	std::istringstream iss(command);
	std::string cmd;
	iss >> cmd;

	if (cmd == "PASS") {
		std::string client_pass;
		iss >> client_pass;
		if (client_pass != password) {
			send_to_client(client_socket, "Wrong password\n");
			remove_client(client_socket);
		} else {
			clients[client_socket] = "authenticated";
			send_to_client(client_socket, "Welcome to IRC server!\n");
		}
	} else if (cmd == "MODE") {
		std::string channel, mode_string, argument;
		iss >> channel >> mode_string;

		if (channels.find(channel) == channels.end()) {
			send_to_client(client_socket, "No such channel: " + channel + "\n");
			return;
		}

		ChannelMode &chan_mode = channel_modes[channel];
		bool is_operator = chan_mode.operators.find(client_socket) != chan_mode.operators.end();

		bool adding = true; // Determine if we're adding or removing modes
		for (size_t i = 0; i < mode_string.length(); i++) {
			char c = mode_string[i];
			if (c == '+') {
				adding = true;
				continue;
			} else if (c == '-') {
				adding = false;
				continue;
			}

			switch (c) {
				case 'i':
					chan_mode.invite_only = adding;
					break;

				case 't':
					chan_mode.topic_restricted = adding;
					break;

				case 'k':
					if (adding) {
						iss >> argument;
						chan_mode.key = argument;
					} else {
						chan_mode.key.clear();
					}
					break;

				case 'o':
					if (is_operator) {
						iss >> argument;

						int target_socket = -1;
						std::map<int, std::string>::iterator nick_it;
						for (nick_it = nicknames.begin(); nick_it != nicknames.end(); ++nick_it) {
							if (nick_it->second == argument) {
								target_socket = nick_it->first;
								break;
							}
						}

						if (target_socket != -1) {
							if (adding) chan_mode.operators.insert(target_socket);
							else chan_mode.operators.erase(target_socket);
						} else {
							send_to_client(client_socket, "No such user: " + argument + "\n");
						}
					} else {
						send_to_client(client_socket, "You are not an operator in this channel.\n");
					}
					break;

				case 'l':
					if (adding) {
						iss >> argument;
						chan_mode.user_limit = std::atoi(argument.c_str());
					} else {
						chan_mode.user_limit = 0;
					}
					break;

				default:
					send_to_client(client_socket, "Unknown mode: " + std::string(1, c) + "\n");
					break;
			}
		}

		send_to_channel(channel, "Mode for channel " + channel + " changed by " + nicknames[client_socket] + "\n", client_socket);
	} else if (cmd == "NICK") {
		std::string nickname;
		iss >> nickname;
		if (is_nickname_taken(nickname)) {
			send_to_client(client_socket, "Nickname is already taken\n");
		} else {
			nicknames[client_socket] = nickname;
			send_to_client(client_socket, "Nickname set to " + nickname + "\n");
		}
	} else if (cmd == "JOIN") {
		std::string channel_name;
		iss >> channel_name;
		join_channel(client_socket, channel_name);
	} else if (cmd == "PRIVMSG") {
		std::string target, msg;
		iss >> target;
		getline(iss, msg);
		if (!msg.empty() && msg[0] == ' ')
			msg = msg.substr(1);
		handle_privmsg(client_socket, target, msg);
	} else if (cmd == "KICK") {
		std::string channel, user;
		iss >> channel >> user;
		// Handle KICK command (operator-only, for example)
		// Check if client is an operator, if not send an error message
	} else if (cmd == "INVITE") {
		std::string user, channel;
		iss >> user >> channel;
		// Handle INVITE command (operator-only, for example)
	} else if (cmd == "TOPIC") {
		std::string channel, topic;
		iss >> channel;
		getline(iss, topic);
		if (!topic.empty() && topic[0] == ' ')
			topic = topic.substr(1);
		ChannelMode &chan_mode = channel_modes[channel];
		// Check if the channel exists
		if (channels.find(channel) == channels.end()) {
			send_to_client(client_socket, "No such channel: " + channel + "\n");
			return;
		}
		// Check if the user is an operator if the channel has topic restriction
		if (chan_mode.topic_restricted && chan_mode.operators.find(client_socket) == chan_mode.operators.end()) {
			send_to_client(client_socket, "You're not allowed to set the topic\n");
			return;
		}
		// Set the topic and notify the channel
		// Assuming a map to store topics exists: channel_topics[channel] = topic;
		send_to_channel(channel, "Topic for channel " + channel + " set to: " + topic + "\n", client_socket);
	} else if (cmd == "QUIT") {
		remove_client(client_socket);
	} else {
		send_to_client(client_socket, "Unknown command\n");
	}
}

void IRCServer::start() {
	while (true) {
		int poll_count = poll(&fds[0], fds.size(), -1);
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

