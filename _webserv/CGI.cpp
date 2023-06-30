# include	"CGI.hpp"
# include	"utils.cpp"

# include	<sstream>
# include	<sys/socket.h>
# include	<arpa/inet.h>
# include	<unistd.h>			// fork, dup2, execve, write, close
# include	<sys/wait.h>		// wait
# include	<fcntl.h>			// open
# include	<fstream>
# include	<iostream>
# include	<cstdio>

//*		Public member functions
CGI::CGI(
	int											sock_fd,
	const std::string&							client_IP,
	const std::string&							server_IP,
	const std::map<std::string, std::string>	req,
	const t_conf_block&							matching_directives,
	std::string									cgi_extension
)
: sock_fd(sock_fd), response(), req(req)
{
	std::cout << "CGI Constructor" << std::endl;
	init_env_paths(take_location_root(matching_directives, false), cgi_extension);
	init_env(matching_directives, client_IP, server_IP);
	print_arr(this->cgi_env, std::string("CGI Environment"));
}

CGI::~CGI()
{
	for (int i = 0 ; i < CGI_ENV_SIZE; i++)
		free(cgi_env[i]);
}

std::vector<char> CGI::getResponse()
{
	return this->response;
}

std::string CGI::get_env_value(const std::string & key){
	for (int i = 0; this->cgi_env[i]; ++i){
		std::string line = std::string(this->cgi_env[i]);
		size_t pos_key = line.find(key + "=");
		if (pos_key == 0)
			return line.substr(key.size() + 1);
	}
	return std::string("");
}

// interpreter
void CGI::launch()
{
	pid_t pid = 0;

	pid = fork();

	if (pid == -1)
		throw SystemCallException("fork()");
	else if (pid == 0)  // child -> CGI
	{		
	// create the input string
		std::string input;
		input = get_env_value("QUERY_STRING");
		if (this->req.find("body") != this->req.end())
			input += this->req.at("body"); // redo
		
	// create an input file and write the content of body and query string
	// update input
		std::ofstream	stream_cgi_input(".cgi_input.txt", std::ios::out | std::ios::trunc);
		if (false == stream_cgi_input.is_open())
			throw SystemCallException("is_open()"); // where, which function, which object

		// add input to stream
		

		if (stream_cgi_input.fail())
			throw SystemCallException("<<"); // where, which function, which object
		stream_cgi_input.close();

	// open input and output files
		int fd_in = open(".cgi_input.txt", O_RDONLY, 0666);
		if (fd_in == -1)
			throw SystemCallException("open()");
		int fd_out = open(".cgi_output.txt", O_RDWR | O_CREAT | O_TRUNC, 0666);
		if (fd_out == -1)
			throw SystemCallException("open()");

	// duping fd // if no body ? dup or not ? -> handled by CGI ?
		if (dup2(fd_in, STDIN_FILENO) == -1)
			throw SystemCallException("dup2(fd_in, STDIN_FILENO");
		if (dup2(fd_out, STDOUT_FILENO) == -1)
			throw SystemCallException("dup2(fd_out, STDOUT_FILENO)");
	// creates arguments for execve
		char* const cmd[] = {
			get_env_value("SCRIPT_NAME").c_str(), // INTERPRETER
			get_env_value("REQUEST_URI").c_str(), // ROOT + SCRIPT-NAME
			nullptr
		};
	// executing cgi
		if (execve(cmd[0], cmd, this->cgi_env) == -1)
			throw SystemCallException("execve()");
	}
	else 	// back to parent
	{              
		if (wait(0) == -1)
			throw SystemCallException("wait()");
		// Update the response
		std::ifstream	stream_cgi_output(".cgi_output.txt", std::ios::in);
		if (false == stream_cgi_output.is_open())
			throw SystemCallException("is_open()"); // where, which function, which object
        
		response.assign(std::istreambuf_iterator<char>(stream_cgi_output), std::istreambuf_iterator<char>());
        
		unlink(".cgi_output.txt"); // remove output file	}
		unlink(".cgi_input.txt"); // remove input file	}
	}
}


//*		Private member functions

// 23	REQUEST_URI		holds the original URI (Uniform Resource Identifier) sent by the client, including the path, query parameters, and fragments
// 24	SCRIPT_NAME		represents the path of the CGI script being executed, relative to the web server's document root
// 15	PATH_INFO		Optionally contains extra path information from the HTTP request that invoked the script, specifying a path to be interpreted by the CGI script. PATH_INFO identifies the resource or sub-resource to be returned by the CGI script, and it is derived from the portion of the URI path following the script name but preceding any query data.
// 17	QUERY_STRING	Contains the query parameters passed in the URL after the "?" character

// https://example.com/cgi_bin/index.php/path/to/resource?query=string",
// REQUEST_URI      would be "/cgi_bin/index.php/path/to/resource?query=string"
// SCRIPT_NAME      would be "cgi_bin/index.php"
// PATH_INFO        would be "path/to/resource"
// QUERY_STRING     would be "query=string"

void CGI::init_env_paths(std::string root, std::string cgi_extension){
	std::string	url(req.at("url"));
	std::string	path_info; 
	std::string	script_name; 
	std::string	query_str; 
	size_t		pos_ext;
	size_t		pos_query;

	pos_ext = url.find(cgi_extension);
	script_name = url.substr(0, pos_ext + cgi_extension.size());
	
	if (url.size() > (pos_ext + cgi_extension.size())){
		path_info = url.substr(pos_ext + cgi_extension.size());
		
		pos_query = path_info.find("?");
		if ((pos_query != std::string::npos) && (pos_query == path_info.size() - 1))
			query_str = path_info.substr(pos_query + 1);
			
		path_info = path_info.substr(0, pos_query);
	}
	path_remove_leading_slash(script_name);
	path_remove_leading_slash(path_info);
	path_remove_leading_slash(query_str);
	
	cgi_env[22] = strdup(   (std::string("REQUEST_URI=")               + 	url).c_str()	);
	cgi_env[23] = strdup(   (std::string("SCRIPT_NAME=")               + 	script_name).c_str()   );
	cgi_env[16] = strdup(   (std::string("QUERY_STRING=")              + 	query_str).c_str()   );
	cgi_env[14] = strdup(   (std::string("PATH_INFO=")                 + 	path_info).c_str()   );
	cgi_env[15] = strdup(   (std::string("PATH_TRANSLATED=")           + 	root + path_info).c_str()   );

}

void	CGI::init_env(const t_conf_block& matching_directives, const std::string& client_IP, const std::string& server_IP){
	// const std::string&	client_IP;	//*	ip of remote client
	// const std::string&	server_IP;	//*	the interface, among all the assigned_server utilized interfaces, where the connection got accepted.

	// Path environment variables declared in init_env_paths();
	cgi_env[0] = strdup(    (std::string("AUTH_TYPE=")                 +	std::string("")).c_str()  );
	cgi_env[1] = strdup(    (std::string("CONTENT_LENGTH=")            +	(req.find("Content-Length") != req.end()) ? req.at("Content-Length") : "").c_str()  );
	cgi_env[2] = strdup(    (std::string("CONTENT_TYPE=")              +	(req.find("Content-Type") != req.end()) ? req.at("Content-Type") : "").c_str()  );
	cgi_env[3] = strdup(    (std::string("GATEWAY_INTERFACE=")         +	std::string("CGI/1.1")).c_str()  );
	cgi_env[4] = strdup(    (std::string("HTTP_ACCEPT=")               +	(req.find("Accept") != req.end()) ? req.at("Accept") : "").c_str()	);
	cgi_env[6] = strdup(    (std::string("HTTP_ACCEPT_ENCODING=")      +	(req.find("Accept-Encoding") != req.end()) ? req.at("Accept-Encoding") : "").c_str()	);
	cgi_env[7] = strdup(    (std::string("HTTP_ACCEPT_LANGUAGE=")      +	(req.find("Accept-Language") != req.end()) ? req.at("Accept-Language") : "").c_str()	);
	cgi_env[8] = strdup(    (std::string("HTTP_COOKIE=")               +	(req.find("Cookie") != req.end()) ? req.at("Cookie") : "").c_str()	);
	cgi_env[9] = strdup(    (std::string("HTTP_FORWARDED=")            +	(req.find("Forwarded") != req.end()) ? req.at("Forwarded") : "").c_str()	);
	cgi_env[10] = strdup(   (std::string("HTTP_HOST=")                 +	(req.find("Host") != req.end()) ? req.at("Host") : "").c_str()	);
	cgi_env[11] = strdup(   (std::string("HTTP_PROXY_AUTHORIZATION=")  +	(req.find("Proxy-Authorization") != req.end()) ? req.at("Proxy-Authorization") : "").c_str()	);
	cgi_env[12] = strdup(   (std::string("HTTP_USER_AGENT=")           +	(req.find("User-Agent") != req.end()) ? req.at("User-Agent") : "").c_str()	);
	cgi_env[13] = strdup(   (std::string("NCHOME=")                    + 	std::string("")).c_str()   );
	cgi_env[17] = strdup(   (std::string("REMOTE_ADDR=")               + 	std::string(client_IP)).c_str()	);
	cgi_env[18] = strdup(   (std::string("REMOTE_HOST=")               + 	std::string("")).c_str()   );
	cgi_env[19] = strdup(   (std::string("REMOTE_IDENT=")              + 	std::string("")).c_str()   );
	cgi_env[20] = strdup(   (std::string("REMOTE_USER=")               + 	std::string("")).c_str()   );
	cgi_env[21] = strdup(   (std::string("REQUEST_METHOD=")            + 	req.at("method")).c_str()   );
	cgi_env[24] = strdup(   (std::string("SERVER_NAME=")               + 	std::string(server_IP)).c_str()				);
	cgi_env[25] = strdup(   (std::string("SERVER_PORT=")               + 	matching_directives.directives.at("listen")).c_str()	);
	cgi_env[26] = strdup(   (std::string("SERVER_PROTOCOL=")           + 	std::string("HTTP/1.1")).c_str()   );
	cgi_env[27] = strdup(   (std::string("SERVER_SOFTWARE=")           + 	std::string("WebServ_PiouPiou/1.0.0 (Ubuntu)")).c_str()   );
	cgi_env[28] = strdup(   (std::string("WEBTOP_USER=")               + 	std::string("")).c_str()   );
	cgi_env[CGI_ENV_SIZE] = NULL;
}

//* Debug
void CGI::print_arr(char ** arr, const std::string& title){
	if (!arr)
		return;
	std::cout << "Printing " << title << std::endl;
	for(int i = 0; arr[i]; ++i)
		std::cout << i << " : " << arr[i] << std::endl;
}
