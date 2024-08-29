# **************************************************************************** #
#                                                                              #
#                                                         :::      ::::::::    #
#    Makefile                                           :+:      :+:    :+:    #
#                                                     +:+ +:+         +:+      #
#    By: edecorce <edecorce@student.42.fr>          +#+  +:+       +#+         #
#                                                 +#+#+#+#+#+   +#+            #
#    Created: 2024/08/14 17:42:17 by edecorce          #+#    #+#              #
#    Updated: 2024/08/29 17:30:08 by edecorce         ###   ########.fr        #
#                                                                              #
# **************************************************************************** #

NAME = ircserv
HEADER_DIR = ircserv.hpp
CPP = c++ -g3
CPPFLAGS = -Wall -Wextra -Werror -std=c++98
RM = rm -f

SOURCES = main.cpp ircserv.cpp srcs/channel.cpp srcs/client.cpp srcs/commands.cpp
OBJECTS = $(SOURCES:.cpp=.o)

%.o: %.cpp $(HEADER_DIR)
	$(CPP) $(CPPFLAGS) -c $< -o $@

all: $(NAME)

$(NAME): $(OBJECTS) $(HEADER_DIR)
	${CPP} -o $(NAME) $(OBJECTS)

clean:
	$(RM) $(OBJECTS)

fclean: clean
	$(RM) $(NAME)

re: fclean all

.PHONY: all clean fclean re
