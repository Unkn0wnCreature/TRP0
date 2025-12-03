#include <sys/types.h>
#include <sys/socket.h>
#include <stdio.h>
#include <stdlib.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <iostream>
#include <string>
#include <cstring>
#include <vector>
#include <sstream>
#include <limits>
#include "matrix.h"
#include "graph.h"
using namespace std;

class Server {
private:
	int server_fd;
	int port;
	bool use_tcp;
public:
	Server(int port, bool use_tcp){
		server_fd = -1;
		this->port = port;
		this->use_tcp = use_tcp;
	}

	~Server(){
		if (server_fd != -1){
			close(server_fd);
		}
	}

	bool start(){
		if (use_tcp){return start_tcp();}
		else {return start_udp();}
	}

private:
	bool start_tcp(){
		int server_fd = socket(AF_INET, SOCK_STREAM, 0);
		if (server_fd == -1){
			std::cerr << "Error to create TCP socket" <<std::endl;
			return false;
		}

		int opt = 1;
		if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt))){
			cerr<<"Error to set options to socket"<<endl;
			close(server_fd);
			return false;
		}

		sockaddr_in address;
		memset(&address, 0, sizeof(address));
		address.sin_family = AF_INET;
		address.sin_addr.s_addr = INADDR_ANY;
		address.sin_port = htons(port);

		if (bind(server_fd, (sockaddr*) &address, sizeof(address)) < 0){
			std::cerr<<"Error to bind TCP socket"<<std::endl;
			close(server_fd);
			return false;
		}

		if (listen(server_fd, 5) < 0){
			std::cerr<<"Error to listen TCP"<<std::endl;
			close(server_fd);
			return false;
		}

		std::cout<<"TCP server launched on port "<< port <<std::endl;
		return handle_tcp_connections(server_fd);
	}

	bool start_udp(){
		int server_fd = socket(AF_INET, SOCK_DGRAM, 0);
		if (server_fd == -1){
			std::cerr<<"Error to create UDP socket"<<std::endl;
			return false;
		}

		sockaddr_in address;
		memset(&address, 0, sizeof(address));
		address.sin_family = AF_INET;
		address.sin_addr.s_addr = INADDR_ANY;
		address.sin_port = htons(port);

		if (bind(server_fd, (sockaddr*) &address, sizeof(address)) < 0){
			std::cerr<<"Error to bind TCP socket"<<std::endl;
			close(server_fd);
			return false;
		}
		
		std::cout<< "UDP server launched on port "<< port <<std::endl;
		return handle_udp_connections(server_fd);
	}

	bool handle_tcp_connections(int server_fd){
		sockaddr_in client_address;
		socklen_t addr_len = sizeof(client_address);

		while (true){
			int client_socket = accept(server_fd, (sockaddr*)&client_address, &addr_len);
			if (client_socket < 0){
				std::cerr<<"Error to accept client"<<std::endl;
				continue;
			}

			char client_ip[INET_ADDRSTRLEN];
			inet_ntop(AF_INET, &client_address.sin_addr, client_ip, INET_ADDRSTRLEN);
			std::cout<<"Client accepted: "<<client_ip<<":"<<ntohs(client_address.sin_port)<<std::endl;
			
			pid_t pid = fork();
			if (pid == 0){
				close(server_fd);
				handle_tcp_client(client_socket);
				close(client_socket);
				exit(0);
			} else if (pid > 0){
				close(client_socket);
			} else {
				std::cerr<<"Error to create process"<<std::endl;
				close(client_socket);
			}
			
		}
		return true;
	}

	bool handle_udp_connections(int server_fd){
		char buffer[1024];
		sockaddr_in client_address;
		socklen_t addr_len = sizeof(client_address);

		while (true){
			memset(buffer, 0, sizeof(buffer));
			ssize_t bytes_received = recvfrom(server_fd, buffer, sizeof(buffer), 0, (sockaddr*)&client_address, &addr_len);
			if (bytes_received > 0){
				//buffer[bytes_received] = '\0';
				char client_ip[INET_ADDRSTRLEN];
				inet_ntop(AF_INET, &client_address.sin_addr, client_ip, INET_ADDRSTRLEN);
				std::cout<<"Received from "<<client_ip<<":"<<ntohs(client_address.sin_port)<<":"<<buffer<<std::endl;

				std::string response = "Echo: ";
				response += buffer;

				sendto(server_fd, response.c_str(), response.length(), 0, (sockaddr*)&client_address, addr_len);
			}
		}
		return true;
	}

	void handle_tcp_client(int client_socket){
		char buffer[1024];

		string welcome = "Соединение с сервером установлено";

		send(client_socket, welcome.c_str(), welcome.length(), 0);

		while (true){
			memset(buffer, 0, sizeof(buffer));
			ssize_t bytes_read = recv(client_socket, buffer, sizeof(buffer)-1, 0);
			if (bytes_read <= 0){
				break;
			}
			
			/*
			if (buffer[bytes_read-1] == '\n'){
				buffer[bytes_read-1] = '\0';
			}
			*/

			//std::cout<<"Получено от: "<<buffer<<std::endl;

			auto matrix = parse_matrix(buffer);

			string response;

			//std::string response = "status:received";
			//response += buffer;

			//send(client_socket, response.c_str(), response.length(), 0);

			if (strncmp(buffer, "exit", 4) == 0){break;}

			//------------------------------------------------------------------
			memset(buffer, 0, sizeof(buffer));
			bytes_read = recv(client_socket, buffer, sizeof(buffer)-1, 0);
			if (bytes_read <= 0){
				break;
			}
			/*
			if (buffer[bytes_read-1] == '\n'){
				buffer[bytes_read-1] = '\0';
			}
			*/
			//std::cout<<"Получено от: "<<buffer<<std::endl;

			auto [a, b] = get_elements(buffer);
			auto [min_dist, path] = dijkstra(matrix, a, b);

			if (min_dist == INF){
				response = "Пути между вершинами не существует";
			} else{
				response = "Результат:  " + convert_len_to_string(min_dist) + "\nКратчайший путь: " + convert_path_to_string(path);
			}

			send(client_socket, response.c_str(), response.length(), 0);

			if (strncmp(buffer, "exit", 4) == 0){break;}

		}
	}
};

int main(int argc, char* argv[]){
	if (argc!= 3){
		std::cout<<"Using:"<<argv[0]<<"<port> <tcp|udp>"<<std::endl;
		return 1;
	}
	int port = std::stoi(argv[1]);
	std::string protocol = argv[2];
	bool use_tcp = (protocol == "tcp");

	if (!use_tcp && protocol != "udp"){
		std::cerr<<"Incorrect protocol"<<std::endl;
		return 1;
	}

	Server server(port, use_tcp);

	if (!server.start()){return 1;}

	return 0;
}
