/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   ircserv.cpp                                        :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: edecorce <edecorce@student.42.fr>          +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2024/08/24 14:59:42 by edecorce          #+#    #+#             */
/*   Updated: 2024/08/29 01:26:47 by edecorce         ###   ########.fr       */
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

void IRCServer::join_channel(int client_socket, const std::string &channel_name, const std::string &channel_password) {
	// Check if the channel already exists
	bool is_new_channel = (channels.find(channel_name) == channels.end());

	// Get or create the channel mode
	ChannelMode &chan_mode = channel_modes[channel_name];

	// If the channel is invite-only, check if the client is allowed
	if (chan_mode.invite_only && !is_new_channel) {
		send_to_client(client_socket, "Channel is invite-only\n");
		return;
	}

	// If the channel has a key (password), prompt the user for it
	if (!chan_mode.key.empty() && !is_new_channel && chan_mode.key != channel_password) {
		send_to_client(client_socket, "Incorrect channel key\n");
		return;
	}

	// Check if the user limit has been reached
	if (chan_mode.user_limit > 0 && channels[channel_name].size() >= (size_t)chan_mode.user_limit) {
		send_to_client(client_socket, "Channel is full\n");
		return;
	}

	// Add the client to the channel
	channels[channel_name].insert(client_socket);

	// If the channel is new, promote the client to operator
	if (is_new_channel) {
		chan_mode.operators.insert(client_socket);
		send_to_client(client_socket, "You are now an operator of channel: " + channel_name + "\n");
	}

	// Notify the client and the channel
	send_to_client(client_socket, "Joined channel: " + channel_name + "\n");
	send_to_channel(channel_name, nicknames[client_socket] + " has joined the channel\n", client_socket);
}



void IRCServer::handle_privmsg(int client_socket, const std::string &target, const std::string &message) {
	if (target[0] == '#') {
		if (channels[target].find(client_socket) == channels[target].end()) {
			send_to_client(client_socket, "You are not in the channel: " + target + "\n");
			return;
		}
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

	// Handle PASS command
	if (cmd == "PASS") {
		std::string client_pass;
		iss >> client_pass;
		if (authenticated_clients[client_socket] == true) {
			send_to_client(client_socket, "You are already authenticated\n");
		} else if (client_pass != password) {
			send_to_client(client_socket, "Wrong password\n");
		} else {
			authenticated_clients[client_socket] = true;
			send_to_client(client_socket, "Welcome to IRC server!\n");
		}
		return;
	}

	// Check if authenticated
	if (authenticated_clients[client_socket] != true) {
		send_to_client(client_socket, "You must authenticate first with PASS.\n");
		return;
	}

	if (cmd != "USER" && cmd != "NICK" &&
		(usernames[client_socket] == "" || nicknames[client_socket] == "")) {

		send_to_client(client_socket, "You must choose a name with USER and a nickname with NICK to go further.\n");
		return;
	}

	if (cmd == "USER") {
		std::string username, hostname, servername, realname;

		// Extract the username
		if (!(iss >> username)) {
			send_to_client(client_socket, "Invalid username\n");
			return;
		}

		// Extract the hostname, servername, and realname if available
		if (!(iss >> hostname >> servername)) {
			send_to_client(client_socket, "Invalid USER command format\n");
			return;
		}

		// Extract the realname, which may contain spaces (hence, using getline)
		getline(iss, realname);
		if (!realname.empty() && realname[0] == ' ') {
			realname = realname.substr(1);  // Remove leading space
		}

		// Store the username (you can store hostname, servername, and realname if needed)
		usernames[client_socket] = username;

		send_to_client(client_socket, "User information set. Welcome " + username + "!\n");
		return;
	}

	// Handle NICK command
	if (cmd == "NICK") {
		std::string nickname;
		iss >> nickname;
		if (nickname.empty()) {
			send_to_client(client_socket, "Invalid nickname\n");
			return;
		}
		// Remove leading space if present
		if (!nickname.empty() && nickname[0] == ' ') {
			nickname = nickname.substr(1);
		}
		
		if (is_nickname_taken(nickname)) {
			send_to_client(client_socket, "Nickname is already taken\n");
		} else {
			nicknames[client_socket] = nickname;
			send_to_client(client_socket, "Nickname set to " + nicknames[client_socket] + "\n");
		}
		return;
	}

	if (cmd == "MODE") {
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
					send_to_client(client_socket, adding ? "Channel is invite-only!\n" : "Channel is not invite-only!\n");
					break;

				case 't':
					chan_mode.topic_restricted = adding;
					send_to_client(client_socket, adding ? "Channel is topic-restricted!\n" : "Channel is not topic-restricted!\n");
					break;

				case 'k':
					if (adding) {
						iss >> argument;
						chan_mode.key = argument;
						send_to_client(client_socket, "Channel password set!\n");
					} else {
						chan_mode.key.clear();
						send_to_client(client_socket, "No password for this channel!\n");
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
							if (adding) {
								chan_mode.operators.insert(target_socket);
								send_to_client(target_socket, nicknames[client_socket] + " added you as an operator of channel: " + channel + "\n");
							}
							else {
								chan_mode.operators.erase(target_socket);
								send_to_client(target_socket, nicknames[client_socket] + " remove you from the operators of channel: " + channel + "\n");
							}
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
						int new_limit = std::atoi(argument.c_str());

						// Check if the new limit is less than the current number of members
						if (new_limit < static_cast<int>(channels[channel].size())) {
							send_to_client(client_socket, "Error: User limit cannot be less than the current number of members\n");
						} else {
							chan_mode.user_limit = new_limit;
							send_to_channel(channel, "User limit for channel " + channel + " set to " + argument + "\n", client_socket);
						}
					} else {
						chan_mode.user_limit = 0;
						send_to_channel(channel, "User limit for channel " + channel + " removed\n", client_socket);
					}
					break;
				default:
					send_to_client(client_socket, "Unknown mode: " + std::string(1, c) + "\n");
					break;
			}
		}
	} else if (cmd == "JOIN") {
		std::string channel_name, channel_password;
		iss >> channel_name >> channel_password;
		if (channel_name.empty() || channel_name[0] != '#') {
			send_to_client(client_socket, "Invalid channel name\n");
			return;
		}
		join_channel(client_socket, channel_name, channel_password);
	} else if (cmd == "PRIVMSG") {
		std::string target, msg;
		iss >> target;
		getline(iss, msg);
		if (msg.empty()) {
			send_to_client(client_socket, "No message to send\n");
			return;
		}
		if (!msg.empty() && msg[0] == ' ')
			msg = msg.substr(1);
		handle_privmsg(client_socket, target, msg);
	} else if (cmd == "KICK") {
		std::string channel, user;
		iss >> channel >> user;

		// Check if the channel exists
		if (channels.find(channel) == channels.end()) {
			send_to_client(client_socket, "No such channel: " + channel + "\n");
			return;
		}

		ChannelMode &chan_mode = channel_modes[channel];

		// Check if the client issuing the KICK command is an operator
		if (chan_mode.operators.find(client_socket) == chan_mode.operators.end()) {
			send_to_client(client_socket, "You are not an operator in this channel.\n");
			return;
		}

		// Find the target user's socket
		int target_socket = -1;
		for (std::map<int, std::string>::iterator it = nicknames.begin(); it != nicknames.end(); ++it) {
			if (it->second == user) {
				target_socket = it->first;
				break;
			}
		}

		if (target_socket == -1) {
			send_to_client(client_socket, "No such user: " + user + "\n");
			return;
		}

		// Check if the user is in the channel
		if (channels[channel].find(target_socket) == channels[channel].end()) {
			send_to_client(client_socket, user + " is not in channel " + channel + "\n");
			return;
		}

		// Remove the user from the channel
		channels[channel].erase(target_socket);

		// Notify the channel that the user was kicked
		send_to_channel(channel, user + " has been kicked by " + nicknames[client_socket] + "\n", client_socket);

		// Notify the kicked user
		send_to_client(target_socket, "You have been kicked from channel " + channel + "\n");
	} else if (cmd == "INVITE") {
		std::string user, channel;
		iss >> user >> channel;

		// Check if the channel exists
		if (channels.find(channel) == channels.end()) {
			send_to_client(client_socket, "No such channel: " + channel + "\n");
			return;
		}

		ChannelMode &chan_mode = channel_modes[channel];

		// Check if it is full
		if (chan_mode.user_limit > 0 && channels[channel].size() >= (size_t)chan_mode.user_limit) {
			send_to_client(client_socket, "Cannot invite anyone as channel is full\n");
			return;
		}
		
		// Check if the client issuing the INVITE command is an operator
		if (chan_mode.operators.find(client_socket) == chan_mode.operators.end()) {
			send_to_client(client_socket, "You are not an operator in this channel.\n");
			return;
		}

		// Find the target user's socket
		int target_socket = -1;
		for (std::map<int, std::string>::iterator it = nicknames.begin(); it != nicknames.end(); ++it) {
			if (it->second == user) {
				target_socket = it->first;
				break;
			}
		}

		if (target_socket == -1) {
			send_to_client(client_socket, "No such user: " + user + "\n");
			return;
		}

		// Add the user to the channel (if the channel is invite-only)
		if (chan_mode.invite_only) {
			channels[channel].insert(target_socket);
			send_to_client(target_socket, "You have been invited to channel " + channel + " by " + nicknames[client_socket] + "\n");
			send_to_client(client_socket, user + " has been invited to channel " + channel + "\n");
			send_to_channel(channel, user + " has been invited to the channel by " + nicknames[client_socket] + "\n", client_socket);
		} else {
			send_to_client(client_socket, "Channel " + channel + " is not invite-only.\n");
		}
	}
	else if (cmd == "TOPIC") {
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

