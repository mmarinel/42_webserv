/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   main.cpp                                           :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: mmarinel <mmarinel@student.42.fr>          +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2023/05/30 13:20:37 by avilla-m          #+#    #+#             */
/*   Updated: 2023/06/29 18:11:52 by mmarinel         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "include/Webserv.hpp"
#include "include/Worker.hpp"
#include "include/Config.hpp"

int main(int ac, char** av)
{(void)ac;
	
	COUT_DEBUG_INSERTION("Welcome to mini-serv" << std::endl);
	try
	{
		Config	config(av[1]);
		
		//* parsing configuration file
		config.parse_config();
		COUT_DEBUG_INSERTION("Configuration parsed\n")
		
		//* initializing worker
		Worker worker(config.getConf());
		COUT_DEBUG_INSERTION("Worker initialized\n")
		
		//* starting worker
		worker.workerLoop();
		COUT_DEBUG_INSERTION("Worker died without exceptions being thrown\n")
	}
	catch (std::exception& e)
	{
		std::cout << std::endl << BOLDRED "main : ! Caught exception >> " << e.what() << RESET << std::endl;
	}
	return (1);
}
