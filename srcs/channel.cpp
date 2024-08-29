#include "../ircserv.hpp"

void IRCServer::send_to_channel(const std::string &channel, const std::string &message, int sender_socket) {
	if (channels.find(channel) == channels.end())
		return;

	for (std::set<int>::iterator it = channels[channel].begin(); it != channels[channel].end(); ++it) {
		if (*it != sender_socket) {
			send_to_client(*it, message);
		}
	}
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
		chan_mode.topic_restricted = true;
		chan_mode.operators.insert(client_socket);
		send_to_client(client_socket, "You are now an operator of channel: " + channel_name + "\n");
	}

	// Notify the client and the channel
	send_to_client(client_socket, "Joined channel: " + channel_name + "\n");
	send_to_channel(channel_name, nicknames[client_socket] + " has joined the channel\n", client_socket);
}