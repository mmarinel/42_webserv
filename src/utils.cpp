/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   utils.cpp                                          :+:      :+:    :+:   */
/*   By: team_PiouPiou                                +:+ +:+         +:+     */
/*       avilla-m <avilla-m@student.42.fr>          +#+  +:+       +#+        */
/*       mmarinel <mmarinel@student.42.fr>        +#+#+#+#+#+   +#+           */
/*                                                     #+#    #+#             */
/*                                                    ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "Webserv.hpp"
#include "Utils.hpp"

#include <unistd.h>		//*access
#include <sys/stat.h>	//*stat
#include <algorithm>	//*std::find
#include <vector>		//*request utils
#include <string>
#include <cstring>
#include <iostream>
#include <sstream>		//*splitting : get_cgi_extension(), etc.
#include <sys/stat.h> 	//*stat
#include <sys/types.h>
#include <dirent.h> 	//*readdir, opendir
#include <cerrno> 		//*errno

// BLOCK HPP
//*		TYPES CONSTRUCTORS
t_conf_block::	s_conf_block(
		t_config_block_level lvl,
		std::map<std::string, std::string> dir
	)
	: level(lvl), directives(dir), sub_blocks(), invalidated(false)
{
	COUT_DEBUG_INSERTION(FULL_DEBUG, "Build a new : " << this->level << std::endl);
}

//*		TYPE UTILITIES
/// printing functions
/*brief*/   // prints contents of a map of string directives
void						print_directives(std::map<std::string, std::string>& directives, size_t level) {
	std::string	tabs(level, '\t');
	
	COUT_DEBUG_INSERTION(FULL_DEBUG, tabs << "| Directives :" << std::endl);
    for (std::map<std::string, std::string>::iterator it = directives.begin(); it != directives.end(); it++)
        COUT_DEBUG_INSERTION(FULL_DEBUG, tabs << "|  Key/Value |" << (*it).first << "|" << (*it).second << "|" << std::endl);
    COUT_DEBUG_INSERTION(FULL_DEBUG, tabs << "--------------------------------------" << std::endl);
}
/*brief*/   // recursive call
void						print_block(t_conf_block& block, size_t level) {
	std::string	tabs(level, '\t');
	
    COUT_DEBUG_INSERTION(FULL_DEBUG, tabs << "--------------------------------------" << std::endl);
    COUT_DEBUG_INSERTION(FULL_DEBUG, tabs << "| Printing print_level #" << block.level << std::endl);
    print_directives(block.directives, level);
    for (std::vector<t_conf_block>::iterator it = block.sub_blocks.begin(); it != block.sub_blocks.end(); it++)
        print_block(*it, level + 1);
}

std::ostream&				operator<<(std::ostream& stream, const t_config_block_level& block) {

	stream << block_get_name(block);

	return (stream);
}

t_config_block_level		next_conf_block_level(t_config_block_level lvl) {
	if (e_root_block == lvl)
		return (e_http_block);
	if (e_http_block == lvl)
		return (e_server_block);
	if (e_server_block == lvl)
		return (e_virtual_server_block);
	if (e_virtual_server_block == lvl)
		return (e_location_block);
	throw (
		std::runtime_error(
			"next_conf_block_level(): no level further than location")
		);
}

//WIP   // check find or compare
int							block_get_level(std::string block_name) {
	if ("root{" == strip_spaces(block_name))
		return e_root_block;
	if ("http{" == strip_spaces(block_name))
		return e_http_block;
	if ("server{" == strip_spaces(block_name))
		return e_server_block;
	if (0 == strncmp("location ", block_name.c_str(), strlen("location ")))
		return e_location_block;
	return -1;
}

std::string 				block_get_name(t_config_block_level level) {
	switch(level) {
		case e_root_block: return "root";
		case e_http_block: return "http";
		case e_server_block: return "server";
		case e_virtual_server_block: return "virtual server";
		case e_location_block: return "location";
	}
	return ("");
}

bool	mandatory_server_directives_present(const t_conf_block& current)
{
	static const char*							mandatory[] = {
		"listen", NULL
	};
	const std::map<std::string, std::string>&	directives
		= current.directives;
	size_t										i;

	i = 0;
	while (mandatory[i])
	{
		if (directives.end() == directives.find(mandatory[i]))
			return (false);
		i++;
	}
	return (true);
}

bool	same_server(
	const t_conf_block& server,
	const t_conf_block& virtual_serv2
)
{
	const std::map<std::string, std::string>&	server_dirs
		= server.sub_blocks[0].directives;
	const std::map<std::string, std::string>&	dirs2
		= virtual_serv2.directives;
		
	return (
		server_dirs.at("listen") == dirs2.at("listen")
		&&
		(
			//*	when host not set, server is listening on all
			//*	available local interfaces (IPs of the machine)
			server_dirs.end() == server_dirs.find("host") ||
			dirs2.end() == dirs2.find("host") ||
			server_dirs.at("host") == dirs2.at("host")

		)
	);
}

/**
 * @brief This function returns true iff there is a conflict between two virtual servers from the configuration.
 * Virtual servers are servers that share the same physical layer (ip and port).
 * The way to distinguish them is the use of server_names which define their virtual host.
 * There cannot be a virtual server that does not specify a server_name;
 * the server_name directive is only optional when there's only one server listening on an ip + port.
 * @param virtual_serv1 
 * @param virtual_serv2 
 * @return true 
 * @return false 
 */
bool	same_host(
	const t_conf_block& virtual_serv1,
	const t_conf_block& virtual_serv2
)
{
	const std::map<std::string, std::string>&	dirs1
		= virtual_serv1.directives;
	const std::map<std::string, std::string>&	dirs2
		= virtual_serv2.directives;
		
	return (
		dirs1.end() == dirs1.find("server_name") ||
		dirs2.end() == dirs2.find("server_name") ||
		str_compare_words(
			dirs1.at("server_name"), dirs2.at("server_name")
		)
	);
}

//*		HTTP utilities
bool	isCGI(
	const std::map<std::string, std::string>&	req,
	const t_conf_block&							matching_directives
)
{
	const std::string cgi_extension = take_cgi_extension(
			req.at("url"),
			matching_directives.directives
		);
		
	return (
		false == cgi_extension.empty()
	);
}

/**
 * @brief 
 * 
 * @param url 
 * @param directives 
 * @return std::string 
 */
std::string	take_cgi_extension(
	const std::string& url,
	const std::map<std::string, std::string>& directives
)
{
	std::string			uri_path = url.substr(0, url.find("?"));
	size_t				cur_dot_pos;
	std::string			cur_extension;
	size_t				ext_found_at_pos;
	std::string			cgi_enable_directive;
	std::stringstream	cgiDirectiveStream;
	COUT_DEBUG_INSERTION(FULL_DEBUG, "take_cgi_extension - uri-path : " << uri_path << std::endl);

	if (directives.end() == directives.find("cgi_enable"))
		return ("");
	cgi_enable_directive = directives.at("cgi_enable");
	cur_dot_pos = 0;
	while (true)
	{
		cur_dot_pos = cgi_enable_directive.find(".", cur_dot_pos);
		if (std::string::npos == cur_dot_pos)
			break ;
		
		cgiDirectiveStream.str(cgi_enable_directive.substr(cur_dot_pos));
		std::getline(cgiDirectiveStream, cur_extension, ' ');
		COUT_DEBUG_INSERTION(FULL_DEBUG, "trying extension : |" << cur_extension << "| " << std::endl);
		ext_found_at_pos = uri_path.find(cur_extension);
		if (
			std::string::npos != ext_found_at_pos &&
			(
				//*script at the end
				ext_found_at_pos + cur_extension.length() == uri_path.length() ||
				//*additional url info
				'/' == uri_path[ext_found_at_pos + cur_extension.length()]
			)
		) {
			COUT_DEBUG_INSERTION(FULL_DEBUG, "returning extension : |" << cur_extension << "| " << std::endl);
			return (cur_extension);
		}
			
		cur_dot_pos += 1;
	}
	return "";
}

/**
 * @brief this function returns the cgi interpreter for the current extension.
 * 
 * @param extension 
 * @param cgi_directive 
 * @return std::string 
 */
std::string		take_cgi_interpreter_path(
						const std::string& extension,
						const std::string& cgi_directive
				)
{
	size_t	extension_pos;
	size_t	interpreter_pos;

	//*	imagine we are parsing a directive like "/usr/bin/bash .bash .sh .zsh /usr/bin/python .py"

	extension_pos = cgi_directive.find(" " + extension);
	interpreter_pos = cgi_directive.rfind(" /", extension_pos);
	if (std::string::npos == interpreter_pos)
		interpreter_pos = 0;
	else
		interpreter_pos += 1;
	
	return (cgi_directive.substr(interpreter_pos, extension_pos - interpreter_pos));
}

std::string		uri_remove_queryString(const std::string& uri)
{
	std::string		uri_path = uri;
	size_t			queryString_pos = uri_path.find("?");

	if (std::string::npos != queryString_pos)
		uri_path.substr(0, queryString_pos);
	
	return (uri_path);
}

//	UTILS EXCEPTIONS

/**
 * This function checks for non directory file accessibility read, write, execute
*/
void			check_file_accessibility(
					int						access_mode,
					const std::string&		fileRelPath,
					const std::string&		location_root,
					const t_conf_block&		matching_directives
				)
{
	const std::string	fileFullPath = location_root + fileRelPath;
	struct stat			fileStat;
	int					statReturn;

	errno = 0;
	statReturn = stat(fileFullPath.c_str(), &fileStat);
	if (
		0 == statReturn && 
		(
			S_ISDIR(fileStat.st_mode) ||
			0 != access(fileFullPath.c_str(), access_mode)
		)
	)
	{
		/*Forbidden*/	throw HttpError(403, matching_directives, location_root);
	}
	if (statReturn < 0)
		throw return_HttpError_errno_stat(location_root, matching_directives);
}

/**
 * file can be a directory
*/
void	check_file_deletable(
			const std::string&		fileRelPath,
			const std::string&		location_root,
			const t_conf_block&		matching_directives
		)
{
	const std::string	filePath = location_root + fileRelPath;
	std::string			dirPath = filePath.substr(0, filePath.rfind("/"));

	if (dirPath.empty())
		dirPath = "./";

// does file exist ? 
	errno = 0;
    if (access(filePath.c_str(), F_OK) == -1) {
        /*Not found*/	throw HttpError(404, matching_directives, location_root);
	}

// do I have the right permission ? 
	errno = 0;
    if (
		-1 == access(dirPath.c_str(), W_OK | X_OK) ||
		-1 == access(filePath.c_str(), W_OK)
	)
	{
		COUT_DEBUG_INSERTION(FULL_DEBUG, RED "Response::deleteFile() : permissions error" RESET << std::endl);
		if (errno == ETXTBSY) // ETXTBSY Write access was requested to an executable which is being executed.
		/*Conflict*/	throw HttpError(409, matching_directives, location_root);
		else
		/*Forbidden*/	throw HttpError(403, matching_directives, location_root);
	}
}

void	check_directory_deletable(
			const std::string&	dirRelPath,
			const std::string&	location_root,
			const t_conf_block&	matching_directives
		)
{COUT_DEBUG_INSERTION(FULL_DEBUG, YELLOW "check_directory_deletable()" RESET << std::endl);
	const std::string	dirFullPath = location_root + dirRelPath;

	check_file_deletable(dirRelPath, location_root, matching_directives);
	errno = 0;
    DIR* dir = opendir(dirFullPath.c_str());
    if (dir){
        dirent* entry;
        while (true)
		{
			errno = 0;
			entry = readdir(dir);
			try {
				if (entry == NULL && errno != 0)								// errno, see above
					/*Server Err*/	throw HttpError(500, matching_directives, location_root);
				else if (entry == NULL)
					break ;
            	if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
					continue;
				COUT_DEBUG_INSERTION(FULL_DEBUG, CYAN 
					<< "| dir: " << dirFullPath 
					<< "| entry : " << entry->d_name 
					<< ((entry->d_type == DT_DIR)? "is a directory" : "") 
					<< (((entry->d_type == DT_REG) || (entry->d_type == DT_LNK))? "is a regular file" : "") 
					<< ((!(entry->d_type == DT_DIR) && !(entry->d_type == DT_REG) && !(entry->d_type == DT_LNK))? "is neither a file nor a directory" : "") 
					<< RESET 
					<< std::endl);
				if (entry->d_type == DT_DIR)
					check_directory_deletable(dirRelPath + "/" + entry->d_name, location_root, matching_directives);
				else if ((entry->d_type == DT_REG) || (entry->d_type == DT_LNK))
					check_file_deletable(dirRelPath + "/" + entry->d_name, location_root, matching_directives);
				else 
					/*Forbidden*/	throw HttpError(403, matching_directives, location_root);
			}
			catch (const HttpError& e) {
				closedir(dir);
				throw (e);
			}
        }
		errno = 0;
        if (closedir(dir) == -1)
			/*Server Err*/	throw HttpError(500, matching_directives, location_root);
    }
    else {
		COUT_DEBUG_INSERTION(FULL_DEBUG, RED "Response::deleteDirectory() : could not open dir error" RESET << std::endl);
		/*Server Err*/	throw HttpError(500, matching_directives, location_root, strerror(errno));
	}
}

HttpError	return_HttpError_errno_stat(
			const std::string& location_root,
			const t_conf_block& matching_directives
		)
{
	const std::string root = location_root;

	switch(errno) // see list of errno errors below
	{
		// 403 Forbidden
		case EACCES:
			throw HttpError(403, matching_directives, root, strerror(errno)); 
		// 414 url too long
		case ENAMETOOLONG:
			throw HttpError(414, matching_directives, root, strerror(errno)); 
		// 500 Internal Server Error
		case EBADF:
		case EFAULT:
		case EINVAL:
		case ELOOP:
		case ENOMEM: 
		case EOVERFLOW:
			throw HttpError(500, matching_directives, root, strerror(errno)); 
		// 404 Not found
		case ENOENT: 
		case ENOTDIR:
			throw HttpError(404, matching_directives, root, strerror(errno));  
		default:
			throw HttpError(400, matching_directives, root, strerror(errno)); 
	}
}
// STAT ERROR
// EACCES|			Search permission is denied for one of the directories in the path prefix of path. (See also path_resolution(7).)
// EBADF|			fd is bad.
// EFAULT|			Bad address.
// ELOOP|			Too many symbolic links encountered while traversing the path.
// ENAMETOOLONG|	path is too long.
// ENOENT|			A component of path does not exist, or path is an empty string.
// ENOMEM|			Out of memory (i.e., kernel memory).
// ENOTDIR|			A component of the path prefix of path is not a directory.
// EOVERFLOW|		path or fd refers to a file whose size, inode number, or number of blocks cannot be represented in,
// 					respectively, the types off_t, ino_t, or blkcnt_t. 
// 					This error can occur when, for example, an application compiled on a 32-bit platform without 
// 					-D_FILE_OFFSET_BITS=64 calls stat() on a file whose size exceeds (1<<31)-1 bytes.


void throw_HttpError_debug(
		std::string function, std::string call,
		int httpStatusCode,
		const t_conf_block & matching_directives, const std::string& location_root
	)
{
	std::cout
		<< RED
		<< "Error : " << function << " : " << call
		<< "http status code : " << httpStatusCode
		<< RESET
		<< std::endl;
	throw (HttpError(httpStatusCode, matching_directives, location_root));
}

//*		GENERAL PURPOSE UTILITIES
bool	hasHttpHeaderDelimiter(std::vector<char>& line)
{
	std::vector<char>::iterator		lf_pos = std::find(line.begin(), line.end(), '\n');

	if (line.end() == lf_pos || line.begin() == lf_pos)//*not found, or cannot go back one position to look for CR
		return (false);
	return (*(lf_pos - 1) == '\r');
}

bool	isHttpHeaderDelimiter(std::vector<char>& line)
{
	return (
		hasHttpHeaderDelimiter(line) &&
		2 == line.size()
	);
}

std::string					strip_spaces(std::string& str) {
	std::string	stripped;

	stripped = str;
	stripped.erase(
		std::remove_if(stripped.begin(), stripped.end(), IsSpacePredicate()),
		stripped.end()
	);
	return (stripped);
}

void						strip_trailing_and_leading_spaces(std::string& str) {
	std::string::iterator			it;
	std::string::reverse_iterator	rit;

	//*		remove all trailing whitespaces up unitl first non-space char
	for (it = str.begin(); it != str.end(); ) {
		if (std::isspace(*it))
		{
			it = str.erase(it);
		}
		else {
			break ;
		}
	}
	//*		remove all leading whitespaces
	for (rit = str.rbegin(); rit != str.rend(); rit++) {
		if (std::isspace(*rit)) {
			it = (rit + 1).base();
			it = str.erase(it);
			rit = std::string::reverse_iterator(it);
		}
		else
			break;
	}
}

/*brief*/	// split a string into a vector using delimiter space, without keeping whitespace
std::vector<std::string> split_str_to_vector( std::string s, const std::string& delimiter) {
	std::vector<std::string>	new_vector;
	std::string 				tmp;
	size_t 						pos;
	while ( s.size() > 0 ){
		strip_trailing_and_leading_spaces(s);
		if (s.empty())
			break ;
		pos = s.find(delimiter, 0);
		if (pos == std::string::npos)
			pos = s.size(); 
		tmp = s.substr(0, pos);
		s.erase(0, pos);
		new_vector.push_back(tmp);
	}
	return (new_vector);
}

/*brief*/	// compare two strings, return true if there is at least one common word
bool	str_compare_words(const std::string& str_haystack, const std::string& str_needle)
{
	std::vector<std::string> vector_haystack = split_str_to_vector(str_haystack, " ");
	std::vector<std::string> vector_needle = split_str_to_vector(str_needle, " ");
	COUT_DEBUG_INSERTION(FULL_DEBUG, MAGENTA"str_compare_words" RESET << std::endl);
	if ((vector_haystack.empty() == true) || (vector_needle.empty() == true)){
		COUT_DEBUG_INSERTION(FULL_DEBUG, MAGENTA"str_compare_words RETURN : negative match VOID VECTOR" RESET << std::endl);
		return (false);
	}

	for (
		std::vector<std::string>::iterator it_needle = vector_needle.begin();
		it_needle != vector_needle.end();
		it_needle++
	){
		for (
			std::vector<std::string>::iterator it_haystack = vector_haystack.begin();
			it_haystack != vector_haystack.end();
			it_haystack++
		){
			COUT_DEBUG_INSERTION(FULL_DEBUG, "comparing |" << *it_needle << "| and |" << *it_haystack << "|" << std::endl);
			if ((*it_haystack).compare(*it_needle) == 0){
				COUT_DEBUG_INSERTION(FULL_DEBUG, MAGENTA"str_compare_words RETURN : positive match" RESET << std::endl);
				return (true);
			}
		}
	}
	COUT_DEBUG_INSERTION(FULL_DEBUG, MAGENTA"str_compare_words RETURN : negative match" RESET << std::endl);
	return (false);
}

void	path_remove_leading_slash(std::string& pathname)
{
	if (pathname.empty() == false && '/' == pathname[0])
		pathname = pathname.substr(1);
}


bool	fileExists(
	const std::string& root,
	std::string path
	)
{COUT_DEBUG_INSERTION(FULL_DEBUG, YELLOW "checking existence of file : " << root + path << RESET << std::endl);
	struct stat	fileStat;
	std::string	fileFullPath;

	path = path.substr(0, path.find("?"));
	path_remove_leading_slash(path);
	fileFullPath = root + path;

	return (0 == stat(fileFullPath.c_str(), &fileStat));
}


bool			isDirectory(const std::string root, std::string path)
{COUT_DEBUG_INSERTION(FULL_DEBUG, YELLOW "isDirectory()" RESET << std::endl);

	struct stat	fileStat;
	
	path_remove_leading_slash(path);
	std::string	dir_path = root + path;

	return (
		0 == stat(dir_path.c_str(), &fileStat) &&
		S_ISDIR(fileStat.st_mode)
	);
}

// readdir()
//		  RETURN VALUE 
//        On success, readdir() returns a pointer to a dirent structure.
//        If the end of the directory stream is reached, NULL is returned
//        and errno is not changed.  If an error occurs, NULL is returned
//        and errno is set to indicate the error.  To distinguish end of
//        stream from an error, set errno to zero before calling readdir()
//        and then check the value of errno if NULL is returned.

std::string getDirectoryContentList(
				const std::string directoryPath,
				const t_conf_block& matching_directives,
				const std::string& location_root
			)
{
	COUT_DEBUG_INSERTION(FULL_DEBUG, MAGENTA << "getDirectoryContentList : path ->"<< directoryPath << RESET << std::endl);
	std::string contentList;
    DIR* dir = opendir(directoryPath.c_str());
    if (dir){
        dirent* entry;
		contentList = "<ul>";
        while (true){
			
			errno = 0;
			entry = readdir(dir);
			if (entry == NULL && errno != 0)		// errno, see above
				throw HttpError(500, matching_directives, location_root);
			else if (entry == NULL)
				break ;
			
			std::string fileType;
			if (entry->d_type == DT_DIR)
				fileType = "<span style=\"color:blue\"> | Directory </span>";
			else if (entry->d_type == DT_REG)
				fileType = "<span style=\"color:green\"> | Regular_file </span>";
			else
				fileType = "<span style=\"color:red\"> | Unkown_type </span>";

            std::string fileName = entry->d_name;
            if (fileName != "." && fileName != ".."){
                contentList += ("<li>" + fileName + "------" + fileType + "</li>");
            }
        }
		contentList += "</ul>";
        if (closedir(dir) != 0)
			throw HttpError(500, matching_directives, location_root);
    }
    else
		throw HttpError(500, matching_directives, location_root);
	COUT_DEBUG_INSERTION(FULL_DEBUG, 
		YELLOW << "contenList" << std::endl
		<< contentList << RESET << std::endl
	);
    return contentList;
}

std::string	createHtmlPage(const std::string& title, const std::string& body)
{
	std::stringstream	pageStream;

	COUT_DEBUG_INSERTION(FULL_DEBUG, 
		YELLOW
		<< "std::string	createHtmlPage() : " << body
		<< RESET << std::endl
	);

	COUT_DEBUG_INSERTION(FULL_DEBUG, 
		"body : " << body << std::endl
	);
	pageStream
	<< "<!DOCTYPE html>\
<html lang=\"en\">\
  <head>\
    <meta charset=\"UTF-8\" />\
    <meta name=\"viewport\" content=\"width=device-width, initial-scale=1.0\" />\
    <title>" << title << "</title>\
  </head>\
  <body>\
    <h1>" << title << "</h1>"
	<< body
	<< "</body>\
</html>\
";

	COUT_DEBUG_INSERTION(FULL_DEBUG, 
		"pageStream.str() : " << pageStream.str() << std::endl
	);
	return (pageStream.str());
}
