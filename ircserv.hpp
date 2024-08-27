/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   ircserv.hpp                                        :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: edecorce <edecorce@student.42.fr>          +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2024/08/15 21:04:02 by edecorce          #+#    #+#             */
/*   Updated: 2024/08/15 21:04:25 by edecorce         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#ifndef IRCSERV_HPP
# define IRCSERV_HPP

# include <iostream>
# include <string>
# include <map>
# include <vector>
# include <set>
# include <sys/socket.h>
# include <netinet/in.h>
# include <arpa/inet.h>
# include <poll.h>
# include <unistd.h>
# include <fcntl.h>
# include <sstream>

# define MAX_CLIENTS 100
# define BUFFER_SIZE 1024

struct ChannelMode {
	bool invite_only;
	bool topic_restricted;
	std::string key;
	int user_limit;
	std::set<int> operators; // Set of operator socket file descriptors
};

class IRCServer {
	private:
		int server_socket;
		std::string password;
		std::map<int, std::string> clients;
		std::map<std::string, std::set<int> > channels;
		std::vector<struct pollfd> fds;
		std::map<int, std::string> nicknames;
		std::map<std::string, ChannelMode> channel_modes;

		void set_non_blocking(int socket);
		void accept_new_client();
		void handle_client(int client_socket);
		void remove_client(int client_socket);
		void process_command(int client_socket, const std::string &command);
		void send_to_client(int client_socket, const std::string &message);
		void send_to_channel(const std::string &channel, const std::string &message, int sender_socket);

		bool is_nickname_taken(const std::string &nickname);
		void join_channel(int client_socket, const std::string &channel_name);
		void handle_privmsg(int client_socket, const std::string &target, const std::string &message);

	public:
		IRCServer(int port, const std::string &password);
		~IRCServer();
		void start();
};

#endif // IRCSERV_HPP
