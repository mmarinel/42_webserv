/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   Worker.hpp                                         :+:      :+:    :+:   */
/*   By: team_PiouPiou                                +:+ +:+         +:+     */
/*       avilla-m <avilla-m@student.42.fr>          +#+  +:+       +#+        */
/*       mmarinel <mmarinel@student.42.fr>        +#+#+#+#+#+   +#+           */
/*                                                     #+#    #+#             */
/*                                                    ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#ifndef Worker_HPP
# define Worker_HPP

// INCLUDES
#include <iostream>         // cin cout cerr
#include <string>           // strings, c_str()
#include <cstring>          // memset

#include <sys/socket.h>     // socket, AF_INET, SOCK_STREAM, recv, bind, socklen_t
#include <netinet/in.h>     // sockaddr_in struct, INADDR_ANY
#include <arpa/inet.h>      // inet_ntop
#include <fcntl.h>          // fcntl
#include <limits.h>			// MAX INT

#include <map>
#include <vector>

#include <unistd.h>         // close
#include <stdlib.h>         // exit, EXIT_FAILURE

#include "Webserv.hpp"
#include "ConnectionSocket.hpp"
#include "EpollData.hpp"

/**
 * @brief The Purpose of this class is to manage multiple servers using a event-driven model.
 * For each server, the worker serves the open connections if there is data to be processed
 * and checks for the accept of new incoming ones.
 * 
 */
class Worker{
//*		Private member attributes ___________________________________
private:
	//*	vector holding t_serv (server conf block with virtual servers sub_blocks + server port and sock fd
	//*	+ vector of open connections) objects.
	VectorServ				servers;
	t_epoll_data			edata;			//*	epoll instance fd + epoll current events 
	long long				cur_memory_usage;	//* current memory usage of the whole webserv

//*		Public member functions _____________________________________
public:
	//*		main Constructors and destructors
				Worker(const t_conf_block& root_block);
				~Worker();
	//*		main functionalities
	void 		workerLoop();

//*		Private member functions ____________________________________
private:
	//* 	Unused canonical form
				Worker(const Worker & cpy);
	Worker&		operator=(const Worker & cpy);
	//*		private helper functions
	void		_io_multiplexing_using_epoll();
	void		_serve_clientS( void );
	void		_handle_new_connectionS( void );
	void		_handle_new_connection();
	//*		private initialization functions
	void		_server_init(const t_conf_block& conf_server_block);
	void		_create_server_socket();
	int			_create_ConnectionSocket(t_server& server, std::string& client_IP, std::string& server_IP);
	void		_epoll_register_ConnectionSocket(int cli_socket);
	void		_set_socket_as_reusable();
	void		_init_server_addr(const std::map<std::string, std::string>& server_directives);
	void		_bind_server_socket_to_ip();
	void		_make_server_listening();
	void		_make_socket_non_blocking(int sock_fd);
	void		_init_io_multiplexing();
	void		_print_server_ip_info();
	std::string	_getIP();

};

#endif