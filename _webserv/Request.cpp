/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   Request.cpp                                        :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: avilla-m <avilla-m@student.42.fr>          +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2023/06/14 17:19:26 by earendil          #+#    #+#             */
/*   Updated: 2023/07/12 12:00:46 by avilla-m         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "include/Request.hpp"
#include <sys/socket.h>	//recv
#include <iostream>		//cout
#include <sstream>		//string stream
#include <string>		//stoi
#include <cstring>		//memset
#include <algorithm>	//std::remove, std::find

//*		main Constructors and Destructors

Request::Request(
	const int sock_fd,
	const t_epoll_data& edata
	) :
	sock_fd(sock_fd),
	edata(edata)
{
	parser_status = e_READING_HEADS;
	cur_body_size = 0;
	chunked = false;
	next_chunk_arrived = false;
	cur_chunk_size = 0;

	req.clear();
	payload.clear();
	sock_stream.clear();
	cur_line.clear();

	clock_gettime(CLOCK_BOOTTIME, &timestamp_start);
	timed_out = false;
}

Request::~Request( void ) {
	req.clear();
	payload.clear();
	memset(rcv_buf, '\0', RCV_BUF_SIZE + 1);
	sock_stream.clear();
	cur_line.clear();
}


//*		Main Functions

std::vector<char>		Request::getIncomingData( void )
{ COUT_DEBUG_INSERTION(YELLOW "Request::getIncomingData()" RESET << std::endl);
	std::vector<char>				incomingData;
	std::vector<char>::iterator		cr_pos;
	std::vector<char>				line;

	COUT_DEBUG_INSERTION("BEFORE\n")
	printVectorChar(payload, "payload");
	parse_line();
	if(payload.empty())
		throw (ChunkNotComplete());
	COUT_DEBUG_INSERTION("AFTER\n")
	printVectorChar(payload, "payload");
	if (false == next_chunk_arrived)
	{
		if (hasHttpHeaderDelimiter(payload))
		{
			cr_pos = std::find(payload.begin(), payload.end(), '\r');
			//*	inserting size line line into 'line'
			line.insert(
				line.begin(),
				payload.begin(),
				cr_pos
			);
			payload.erase(payload.begin(), cr_pos + 2);//*taking line off the payload

			line.push_back('\0');
			cur_chunk_size = std::stoi(line.data(), nullptr, 16);
			COUT_DEBUG_INSERTION("current chunk size : " << cur_chunk_size << std::endl);
			next_chunk_arrived = true;
		}
		throw (ChunkNotComplete());
	}
	else
	{
		if (payload.size() < cur_chunk_size + 2)
			throw (ChunkNotComplete());
		if (
			payload[cur_chunk_size] != '\r' ||
			payload[cur_chunk_size + 1] != '\n' 
		)
			throw std::invalid_argument("Invalid Chunk read");
		incomingData.clear();
		incomingData.insert(
			incomingData.begin(),
			payload.begin(),
			payload.begin() + cur_chunk_size
		);
		payload.erase(
			payload.begin(),
			payload.begin() + cur_chunk_size + 2
		);
		next_chunk_arrived = false;
		printVectorChar(incomingData, "Chunk bytes");

		return (incomingData);
	}
}

const std::map<std::string, std::string>&	Request::getRequest( void ) {
	return (this->req);
}

const std::vector<char>&	Request::getPayload( void ) {
	return (this->payload);
}

bool						Request::isChunked( void ) {
	return (this->chunked);
}

bool						Request::isRequestTimeout( void ){
    struct timespec	timestamp_now;
	double			elapsed_secs;
	const double	timeout_value = 05.0; // 5 seconds
	long			timestamp_now_us;
	long			timestamp_start_us;
	
	clock_gettime(CLOCK_BOOTTIME, &timestamp_now);
	timestamp_start_us	= (this->timestamp_start.tv_sec * pow(10, 9)) + this->timestamp_start.tv_nsec;
	timestamp_now_us	= (timestamp_now.tv_sec * pow(10, 9)) + timestamp_now.tv_nsec;
    elapsed_secs		= static_cast<double>(timestamp_now_us - timestamp_start_us) / pow(10, 9);
	
	return (elapsed_secs > timeout_value);
}

void	Request::parse_line( void )
{
	const struct epoll_event*	eevent = edata.getEpollEvent(this->sock_fd);
	
	if (this->isRequestTimeout()) {
		timed_out = true;
		req["method"] = "UNKNOWN";
		req["url"] = "/";
		req["http_version"] = "HTTP/1.1";
		throw TaskFulfilled();
	}
	if (
		false == sock_stream.empty() ||
		(NULL != eevent &&  eevent->events & EPOLLIN))
	{
		read_line();
		//! printVectorChar(cur_line, "cur_line");
		if (e_READING_HEADS == parser_status) {
			parse_header();
		}
		else
		if (e_READING_BODY == parser_status) {
			parse_body();
		}
	}
}

void	Request::read_line( void )
{
	const struct epoll_event*	eevent = edata.getEpollEvent(this->sock_fd);
	int							bytes_read;

	//*	Checking for incoming data and writing into statically allocated recv buffer
	if (NULL != eevent && (eevent->events & EPOLLIN))
	{
		memset(rcv_buf, '\0', RCV_BUF_SIZE + 1);
		bytes_read = recv(sock_fd, rcv_buf, RCV_BUF_SIZE, 0);
		if (bytes_read <= 0)
			throw (SockEof());
		
		//*	Dumping into dynamic entity for character handling (stream)
		sock_stream.insert(
			sock_stream.end(),
			rcv_buf,
			rcv_buf + bytes_read
		);
	}

	//*	Read characters from dynamic entity for character handling (stream)
	if (e_READING_HEADS == parser_status) {
		read_header();
	}
	else
	if (e_READING_BODY == parser_status) {
		read_body();
	}
}

void	Request::read_header( void )
{
	std::vector<char>::iterator		lf_pos;

	if (hasHttpHeaderDelimiter(sock_stream))
	{
		lf_pos = std::find(sock_stream.begin(), sock_stream.end(), '\n');
		cur_line.insert(
			cur_line.end(),
			sock_stream.begin(),
			lf_pos + 1
		);
		sock_stream.erase(sock_stream.begin(), lf_pos + 1);
	}
}

void	Request::read_body( void ) {

	std::size_t		bytes_read = sock_stream.size();

	cur_line.insert(
		cur_line.end(),
		sock_stream.begin(),
		sock_stream.end()
	);
	sock_stream.clear();
	//! std::cout << "read " << bytes_read << " bytes of body" << std::endl;
	if (false == chunked)
		cur_body_size -= bytes_read;
}

void	Request::parse_header( void )
{
	if (false ==  cur_line.empty()) {
		if (isHttpHeaderDelimiter(cur_line))
		{
			//*	end of headers, there may or not may be a body
			cur_line.clear();
			if (req.end() == req.find("method")) {
				throw SockEof();
			}
			else
			if (req.end() != req.find("Content-Length"))
			{
				parser_status = e_READING_BODY;
				cur_body_size = std::atol(req["Content-Length"].c_str());
				//! std::cout << "cur_body_size : " << cur_body_size << std::endl;
				if (0 == cur_body_size)
					throw TaskFulfilled();
			}
			else {
				if (chunked)
					parser_status = e_READING_BODY;
				throw TaskFulfilled();
			}
		}
		else {
			cur_line.erase(std::remove(cur_line.begin(), cur_line.end(), '\r'),  cur_line.end());
			cur_line.erase(std::remove(cur_line.begin(), cur_line.end(), '\n'),  cur_line.end());
			cur_line.push_back('\0');
			if (req.empty()) {
				//*		request is empty, first line is request line
				parse_req_line(cur_line);
			}
			else {
				//*		request is not empty, line is header line
				std::stringstream	str_stream(cur_line.data());
				std::string			key;
				std::string			value;
				
				std::getline(str_stream, key, ':');
				std::getline(str_stream, value);
				strip_trailing_and_leading_spaces(value);
				req[key] = value;
				if (
					"Transfer-Encoding" == key &&
					"chunked" == value
				) 
					chunked = true;
			}
		}
		cur_line.clear();
	}
}

void	Request::parse_req_line( std::vector<char>& req_line ) {

	std::stringstream	reqline_stream(req_line.data());
	std::string			method;
	std::string			url;
	std::string			http_version;

	std::getline(reqline_stream, method, ' ');
	std::getline(reqline_stream, url, ' ');
	std::getline(reqline_stream, http_version);
	req["method"] = method;
	req["url"] = url;
	req["http_version"] = http_version;

	if (std::string::npos != req["url"].find("http://"))
		req["url"].substr(7);
}

void	Request::parse_body( void ) {
	
	payload.insert(
		payload.end(),
		cur_line.begin(),
		cur_line.end()
	);
	cur_line.clear();
	if (false == chunked && 0 == cur_body_size)
	{
		throw TaskFulfilled();
	}
}



//*		helper functions

void	Request::print_req( void ) {
	COUT_DEBUG_INSERTION( "| START print_req |" << std::endl );

	COUT_DEBUG_INSERTION( BOLDGREEN "PRINTING HEADERS" RESET << std::endl );
	for (std::map<std::string, std::string>::iterator it = req.begin(); it != req.end(); it++) {
		COUT_DEBUG_INSERTION( "|" << it->first << " : " << it->second << "|" << std::endl );
	}
	COUT_DEBUG_INSERTION( GREEN "END---PRINTING HEADERS" RESET << std::endl );

	COUT_DEBUG_INSERTION( BOLDGREEN "PRINTING body" RESET << std::endl );
	// for (std::vector<char>::iterator it = payload.begin(); it != payload.end(); it++)
	// 	COUT_DEBUG_INSERTION( *it );
	// COUT_DEBUG_INSERTION( std::endl );
	COUT_DEBUG_INSERTION( "body len : " << payload.size() << std::endl );
	COUT_DEBUG_INSERTION( GREEN "END---PRINTING body" RESET << std::endl );

	COUT_DEBUG_INSERTION( "| END print_req |" << std::endl);
}

void	Request::printVectorChar(std::vector<char>& v, std::string name)
{
	if (v.empty())
		std::cout << name << " is empty !" << std::endl;
	else
	{
		std::cout << name << ": " << std::endl;
		for (std::vector<char>::iterator it = v.begin(); it != v.end(); it++)
		{
			if ((*it) == '\n')
				std::cout << RED "n" RESET;
			else
			if ((*it) == '\r')
				std::cout << RED "r" RESET;
			else
				std::cout << *it ;
		}
		std::cout << std::endl;
	}
}

bool	Request::timedOut( void )
{
	return (this->timed_out);
}
