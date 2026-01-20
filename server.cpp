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
#include <thread>
#include <mutex>
#include <map>
#include "matrix.h"
#include "graph.h"
using namespace std;

class Server {
private:
	int server_fd;
	int port;
	bool use_tcp;
	vector<thread> threads;
	mutex console_mutex;
	mutex clients_mutex;
	map<string, bool> active_clients;

	int client_counter = 0;
	mutex counter_mutex;

	const int MAX_RETRIES = 3;
	const int TIMEOUT_SEC = 3;
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

		for (auto& thread : threads){
			if (thread.joinable()){thread.join();}
		}
	}

	bool start(){
		if (use_tcp){return start_tcp();}
		else {return start_udp();}
	}

private:
	bool start_tcp(){
		server_fd = socket(AF_INET, SOCK_STREAM, 0);
		if (server_fd == -1){
			std::cerr << "Ошибка создания TCP сокета" <<std::endl;
			return false;
		}

		int opt = 1;
		if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt))){
			cerr<<"Ошибка установки настроек сокета"<<endl;
			close(server_fd);
			return false;
		}

		sockaddr_in address;
		memset(&address, 0, sizeof(address));
		address.sin_family = AF_INET;
		address.sin_addr.s_addr = INADDR_ANY;
		address.sin_port = htons(port);

		if (bind(server_fd, (sockaddr*) &address, sizeof(address)) < 0){
			std::cerr<<"Ошибка привязки TCP сокета"<<std::endl;
			close(server_fd);
			return false;
		}

		if (listen(server_fd, 5) < 0){
			std::cerr<<"Ошибка прослушивания TCP"<<std::endl;
			close(server_fd);
			return false;
		}
		cout<<"TCP сервер запущен на порту: " << port <<endl;	
		return handle_tcp_connections(server_fd);
	}

	bool start_udp(){
		server_fd = socket(AF_INET, SOCK_DGRAM, 0);
		if (server_fd == -1){
			std::cerr<<"Ошибка создания UDP сокета"<<std::endl;
			return false;
		}

		sockaddr_in address;
		memset(&address, 0, sizeof(address));
		address.sin_family = AF_INET;
		address.sin_addr.s_addr = INADDR_ANY;
		address.sin_port = htons(port);

		if (bind(server_fd, (sockaddr*) &address, sizeof(address)) < 0){
			std::cerr<<"Ошибка привязки UDP сокета"<<std::endl;
			close(server_fd);
			return false;
		}
		
		std::cout<< "UDP сервер запущен на порту "<< port <<std::endl;
		return handle_udp_connections(server_fd);
	}

	bool handle_tcp_connections(int server_fd){
		sockaddr_in client_address;
		socklen_t addr_len = sizeof(client_address);

		while (true){
			int client_socket = accept(server_fd, (sockaddr*)&client_address, &addr_len);
			if (client_socket < 0){
				std::cerr<<"Ошибка принятия клиента"<<std::endl;
				continue;
			}

			char client_ip[INET_ADDRSTRLEN];
			inet_ntop(AF_INET, &client_address.sin_addr, client_ip, INET_ADDRSTRLEN);

			//----
			int current_client_id;
			{
				lock_guard<mutex> lock(console_mutex);
				current_client_id = ++client_counter;
			}

			{
				lock_guard<mutex> lock(console_mutex);
				cout<<"Подключён клиент "<< current_client_id <<" : "<< client_ip<<" : "<<ntohs(client_address.sin_port)<<" : "<< threads.size() << " потоков активно"<<endl;
			}

			threads.emplace_back([this, client_socket, current_client_id, client_address]() {
					handle_tcp_client(client_socket);

					{
						lock_guard<mutex> lock(console_mutex);
						cout<<"Поток для клиента "<< current_client_id <<" завершён"<<endl;
					}
				});
			//---
				if (threads.size() > 100){
				threads.erase(remove_if(threads.begin(), threads.end(),[](const thread& t) {
					return !t.joinable();
					}), 
					threads.end());
			}		
		}
		return true;
	}

	bool handle_udp_connections(int server_fd){
		char buffer[1024];
		string welcome = "Соединение с сервером установлено";
		sockaddr_in client_address;
		socklen_t addr_len = sizeof(client_address);

		while (true){
			memset(buffer, 0, sizeof(buffer));
			addr_len = sizeof(client_address);

			struct timeval timeout;
			timeout.tv_sec = 5;
			timeout.tv_usec = 0;
			setsockopt(server_fd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));

			
			ssize_t bytes_received = recvfrom(server_fd, buffer, sizeof(buffer), 0, (sockaddr*)&client_address, &addr_len);
			
			timeout.tv_sec = 0;
			timeout.tv_usec = 0;
			setsockopt(server_fd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));

			
			if (bytes_received <= 0){
				continue;
			}

			string received_data(buffer, bytes_received);
			
			/*
			char client_ip[INET_ADDRSTRLEN];
			inet_ntop(AF_INET, &client_address.sin_addr, client_ip, INET_ADDRSTRLEN);
			string client_key = string(client_ip) + ":" + to_string(ntohs(client_address.sin_port));

			{
				lock_guard<mutex> lock(clients_mutex);
				if (active_clients.find(client_key) == active_clients.end()){
					string welcome = "Connection established";
					sendto(server_fd, welcome.c_str(), welcome.length(), 0, (sockaddr*)&client_address, addr_len);
					cout<<"Server welcome message to "<< client_key <<endl;
					active_client[client_key] = true;
					//active_clients.erase(client_key);
				}
			}
				continue;
			}
			*/

			char client_ip[INET_ADDRSTRLEN];
			inet_ntop(AF_INET, &client_address.sin_addr, client_ip, INET_ADDRSTRLEN);

			string client_key = string(client_ip) + ":" + to_string(ntohs(client_address.sin_port));

			{
				lock_guard<mutex> lock(clients_mutex);
				if (active_clients.find(client_key) != active_clients.end()){
					string welcome = "Connection established";
					sendto(server_fd, welcome.c_str(), welcome.length(), 0, (sockaddr*)&client_address, addr_len);
					cout<<"Server welcome message to "<< client_key <<endl;

					active_clients[client_key] = true;
				}
			}

			if (received_data == "1" || received_data == "ASK"){
				continue;
			}

			if (received_data == "exit"){
				lock_guard<mutex> lock(clients_mutex);
				active_clients.erase(client_key);
				continue;
			}

			int current_client_id;
			{
				lock_guard<mutex> lock(counter_mutex);
				current_client_id = ++client_counter;
			}

			{
				lock_guard<mutex> lock(console_mutex);
				cout<<"Подключён UDP клиент " << current_client_id <<" : "<< client_key <<" : " << threads.size() <<" потоков активно"<<endl;
			}

			{
				threads.emplace_back([this, server_fd, client_address, client_key, current_client_id, received_data](){

					socklen_t addr_len = sizeof(client_address);
					string ask = "ASK";
					sendto(server_fd, ask.c_str(), ask.length(), 0, (sockaddr*)&client_address, addr_len);

					handle_udp_client(server_fd, client_address, client_key, current_client_id, received_data);
					{
						lock_guard<mutex> lock(console_mutex);
						cout<<"Поток для UDP клиента "<< current_client_id <<" завершён"<<endl;
					}

					{
						lock_guard<mutex> lock(clients_mutex);
						active_clients.erase(client_key);
					}
				});
			}

			if (threads.size() > 100){
				threads.erase(remove_if(threads.begin(), threads.end(), [](const thread& t){
					return !t.joinable();
					}), threads.end());
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
			
			ssize_t bytes_received = recv(client_socket, buffer, 1024, 0);
			if (bytes_received <= 0 || buffer == "exit"){
				cout<<"Клиент отключился"<<endl;
				break;
			}

			auto [matr, dot] = read_data(buffer);
			replace(dot.begin(), dot.end(), '|', ' ');			

			auto matrix = parse_matrix(matr.c_str());

			string response;

			//------------------------------------------------------------------
			memset(buffer, 0, sizeof(buffer));
				
			auto [a, b] = get_elements(dot.c_str());
			auto [min_dist, path] = dijkstra(matrix, a-1, b-1);

			if (min_dist == INF){
				response = "Пути между вершинами не существует";
			} else{
				response = "Результат:  " + convert_len_to_string(min_dist) + "\nКратчайший путь: " + convert_path_to_string(path);
			}

			if (!send_tcp(client_socket, response)){
				cout<<"Ошибка отправки данных"<<endl;
			}
		}
	}

	void handle_udp_client(int server_fd, sockaddr_in client_address, string client_key, int client_id, const string& initial_data = ""){
		char buffer[1024];
		string welcome = "Соединение с сервером установлено";
		socklen_t addr_len = sizeof(client_address);

		if (!initial_data.empty()){
			strcpy(buffer, initial_data.c_str());

			auto [matr, dot] = read_data(buffer);
			replace(dot.begin(), dot.end(), '|', ' ');

			auto matrix = parse_matrix(matr.c_str());
			string response;

			auto [a, b] = get_elements(dot.c_str());
			auto [min_dist, path] = dijkstra(matrix, a-1, b-1);

			if (min_dist == INF){
				response = "No way";
			}
			response = "Result: " + convert_len_to_string(min_dist) + "\nShortest path: " + convert_path_to_string(path);

			send_udp(server_fd, response, client_address);
		}

		while (true){
			memset(buffer, 0, sizeof(buffer));
			
			if (!receive_udp(server_fd, buffer, sizeof(buffer), client_address)){continue;}

			if (string(buffer) == "exit"){
				cout<<"Клиент отключился"<<endl;
				break;
			}

			auto [matr, dot] = read_data(buffer);
			replace(dot.begin(), dot.end(), '|', ' ');

			auto matrix = parse_matrix(matr.c_str());
			string response;

			auto [a, b] = get_elements(dot.c_str());
			auto [min_dist, path] = dijkstra(matrix, a-1, b-1);

			if (min_dist == INF){
				response = "Пути между вершинами не существует";
			} else{
				response = "Результат:  " + convert_len_to_string(min_dist) + "\nКратчайший путь: " + convert_path_to_string(path);
			}
		
			//ssize_t bytes_sent = sendto(server_fd, response.c_str(), response.length(), 0, (sockaddr*)&client_address, addr_len);
			if (!send_udp(server_fd, response, client_address)){break;}
			}
	}

	bool send_tcp(int sockfd, const string& message){
		ssize_t bytes_sent = send(sockfd, message.c_str(), message.length(), 0);

		return (bytes_sent > 0);
	}

	bool receive_tcp(int sockfd, char buffer[1024]){
		ssize_t bytes_received = recv(sockfd, buffer, 1024, 0);

		return (bytes_received > 0);
	}
	
	bool send_udp(int sockfd, const string& message, sockaddr_in client_address){
		char ask_buffer[10];
		socklen_t addr_len = sizeof(client_address);
		
		struct timeval timeout;
		timeout.tv_sec = TIMEOUT_SEC;
		timeout.tv_usec = 0;
		setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));

		for (int attempt = 0; attempt < MAX_RETRIES; attempt++){
			ssize_t bytes_sent = sendto(sockfd, message.c_str(), message.length(), 0, (sockaddr*)&client_address, addr_len);
			cout<<"Server sent: "<<message<<endl;

			if (bytes_sent <= 0){
				cout<<"Ошибка отправки (попытка "<< (attempt+1) <<")"<<endl;
				continue;
			}

			memset(ask_buffer, 0, sizeof(ask_buffer));
			//sleep(1);
			ssize_t ask_received = recvfrom(sockfd, ask_buffer, sizeof(ask_buffer), 0, (sockaddr*)&client_address, &addr_len);
			cout<<"Server received: "<<ask_buffer<<endl;

			timeout.tv_sec = 0;
			timeout.tv_usec = 0;
			setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));

			if (ask_received > 0){
				if (string(ask_buffer) == "ASK"){
					timeout.tv_sec = 0;
					timeout.tv_usec = 0;
					setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));

					return true;
				}
			} else {
				cout<<"Timeout to acknowledge"<<endl;
			}

			if (attempt < MAX_RETRIES - 1){
				sleep(1);
				cout<<"Повторная отправка (попытка "<< (attempt+1) <<")"<<endl;
			}
		}		
		timeout.tv_sec = 0;
		timeout.tv_usec = 0;
		setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));

		cout<<"Потеряна связь с клиентом"<<endl;
		return false;
	}

	bool receive_udp(int sockfd, char* buffer, size_t buffer_size, sockaddr_in client_address){
		socklen_t addr_len = sizeof(client_address);

		struct timeval original_timeout, timeout;
		timeout.tv_sec = 2;
		timeout.tv_usec = 0;
		setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));
		sleep(1);

		ssize_t bytes_received = recvfrom(sockfd, buffer, buffer_size, 0, (sockaddr*)&client_address, &addr_len);
		cout<<"Server received: "<<buffer<<endl;

		//setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, &original_timeout, sizeof(original_timeout));
		timeout.tv_sec = 0;
		timeout.tv_usec = 0;
		setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));

		if (bytes_received > 0){
			string ask = "ASK";
			sendto(sockfd, ask.c_str(), ask.length(), 0, (sockaddr*)&client_address, addr_len);
			cout<<"Server sent: "<<ask<<endl;
			return true;
		}
		return false;
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
		std::cerr<<"Неверный протокол"<<std::endl;
		return 1;
	}

	Server server(port, use_tcp);

	if (!server.start()){return 1;}

	return 0;
}
