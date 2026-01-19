#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <iostream>
#include <cstring>
#include <string>
#include <algorithm>
#include "matrix.h"
#include "graph.h"
#include "file_manager.h"
using namespace std;


class Client {
private:
	int sockfd;
	bool use_tcp;
	string server_ip;
	int port;
	sockaddr_in server_address;

	const int MAX_RETRIES = 3;
	const int TIMEOUT_SEC = 3;
public:
	Client(const string& ip, int port, bool use_tcp){
		sockfd = -1;
		this->server_ip = ip;
		this->port = port;
		this->use_tcp = use_tcp;

		memset(&server_address, 0, sizeof(server_address));
		server_address.sin_family = AF_INET;
		server_address.sin_port = htons(port);
		inet_pton(AF_INET, server_ip.c_str(), &server_address.sin_addr);

	}

	~Client(){
		if (sockfd != -1){
			close(sockfd);
		}
	}

	bool connect_to_server(){
		if (use_tcp){
			return connect_tcp();
		} else{
			return connect_udp();
		}
	}

	void run(){
		if (use_tcp){
			run_tcp();
		} else{
			run_udp();
		}
	}

private:
	bool connect_tcp(){
		sockfd = socket(AF_INET, SOCK_STREAM, 0);
		if (sockfd == -1){
			cerr<<"Error to create tcp socket"<<endl;
			return false;
		}

		struct timeval timeout;
		timeout.tv_sec = 5;
		timeout.tv_usec = 0;

		if (setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout)) < 0){
			cout<<"Ошибка присвоения таймаута на получение"<<endl;
			return false;
		}

		if (connect(sockfd, (sockaddr*)&server_address, sizeof(server_address)) < 0){
			cerr<<"Не удалось подключиться к серверу"<<endl;
			close(sockfd);
			sockfd = -1;
			return false;
		}

		return true;
	}

	bool connect_udp(){
		sockfd = socket(AF_INET, SOCK_DGRAM, 0);
		if (sockfd == -1){
			cerr<<"Error to create UDP socket"<<endl;
			return false;
		}

		struct timeval timeout;
		timeout.tv_sec = 5;
		timeout.tv_usec = 0;

		if (setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout)) < 0){
			cout<<"Ошибка присвоения таймаута на получение"<<endl;
			return false;
		}

		return true;
	}

	void run_tcp(){
		string message, data, source, filename;
		char buffer[1024];
		ssize_t bytes_received = recv(sockfd, buffer, sizeof(buffer), 0);
		if (bytes_received > 0){
			cout<< buffer <<endl;
		}

		while (true){
			cout<<"\nИсточник данных:"<<endl;
			getline(cin, source);

			if (source == "keyboard"){
				cout<<"\nВведите описание графа:" <<endl;
				getline(cin, message);

				if (message.empty()){continue;}
			
				if (message != "exit"){
					if (!isValidMatrix(message.c_str())){
						cout<<"\nНеверный формат ввода графа"<<endl;
						continue;
					}

					if (!matrix_is_correct(message.c_str(), 2)){
						cout<<"\nГраф должен содержать не менее 6 вершин и 6 рёбер"<<endl;
						continue;
					}
				} else {
					break;
				}

				auto current_matrix = parse_matrix(message.c_str());

				cout<<"\nВведите начальную и конечную вершины:"<<endl;
				getline(cin, data);

				if (data.empty()){continue;}

				if (data != "exit"){
					auto [a, b] = get_elements(data.c_str());
					int max = (a<=b ? b : a);
					int min = (a<=b ? a : b);

					if (min <= 0 || max > current_matrix.size()){
						cout<<"\nВершины не найдены в графе\n"<<endl;
						continue;
					}
				} else {
					break;
				}
			} else if (source == "file"){
				cout<<"\nEnter filename"<<endl;
				getline(cin, filename);

				auto [m, d] = read_from_file(filename);

				message = m;
				data = d;
				
				if (message.empty()){continue;}
			
				if (message != "exit"){
					if (!isValidMatrix(message.c_str())){
						cout<<"\nНеверный формат ввода графа"<<endl;
						continue;
					}

					if (!matrix_is_correct(message.c_str(), 2)){
						cout<<"\nГраф должен содержать не менее 6 вершин и 6 рёбер"<<endl;
						continue;
					}
				} else {
					break;
				}

				auto current_matrix = parse_matrix(message.c_str());

				if (data.empty()){continue;}

				if (data != "exit"){
					auto [a, b] = get_elements(data.c_str());
					int max = (a<=b ? b : a);
					int min = (a<=b ? a : b);

					if (min <= 0 || max > current_matrix.size()){
						cout<<"\nВершины не найдены в графе\n"<<endl;
						continue;
					}
				} else {
					break;
				}
			} else if (source == "exit"){
				break;
			} else {
				cout<<"\nInvalid source"<<endl;
				continue;
			}
			
			auto current_matrix = parse_matrix(message.c_str());
			replace(data.begin(), data.end(), ' ', '|');
			string input = message + "|" + data;

			if (!send_tcp(sockfd, input)){
				cout<<"error to send"<<endl;
				break;
			}
			//-------------------------------------------------------------------
			memset(buffer, 0, sizeof(buffer));

			if (!receive_tcp(sockfd, buffer)){
				cout<<"server disconnected"<<endl;
			}
			cout<<"\n"<< buffer <<endl;
		}

	}

	void run_udp(){
		string message, data, source, filename;
		string check = "1";
		char buffer[1024];
		socklen_t addr_len = sizeof(server_address);
		ssize_t bytes_sent = sendto(sockfd, check.c_str(), sizeof(check), 0, (sockaddr*)&server_address, addr_len);
		
		while (true){
			cout<<"\nИсточник данных:"<<endl;
			getline(cin, source);

			if (source == "keyboard"){

				cout<<"\nВведите описание графа:" <<endl;
				getline(cin, message);

				if (message.empty()){continue;}
			
				if (message != "exit"){
					if (!isValidMatrix(message.c_str())){
						cout<<"\nНеверный формат ввода графа"<<endl;
						continue;
					}

					if (!matrix_is_correct(message.c_str(), 2)){
						cout<<"\nГраф должен содержать не менее 6 вершин и 6 рёбер"<<endl;
						continue;
					}
				} else {
					break;
				}

				auto current_matrix = parse_matrix(message.c_str());

				cout<<"\nВведите начальную и конечную вершины:"<<endl;
				getline(cin, data);

				if (data.empty()){continue;}

				if (data != "exit"){
					auto [a, b] = get_elements(data.c_str());
					int max = (a<=b ? b : a);
					int min = (a<=b ? a : b);

					if (min <= 0 || max > current_matrix.size()){
						cout<<"\nВершины не найдены в графе\n"<<endl;
						continue;
					}
				} else {
					break;
				}
			} else if (source == "file"){
				cout<<"\nEnter filename"<<endl;
				getline(cin, filename);

				auto [m, d] = read_from_file(filename);

				message = m;
				data = d;
				
				if (message.empty()){continue;}
			
				if (message != "exit"){
					if (!isValidMatrix(message.c_str())){
						cout<<"\nНеверный формат ввода графа"<<endl;
						continue;
					}

					if (!matrix_is_correct(message.c_str(), 2)){
						cout<<"\nГраф должен содержать не менее 6 вершин и 6 рёбер"<<endl;
						continue;
					}
				} else {
					break;
				}

				auto current_matrix = parse_matrix(message.c_str());

				if (data.empty()){continue;}

				if (data != "exit"){
					auto [a, b] = get_elements(data.c_str());
					int max = (a<=b ? b : a);
					int min = (a<=b ? a : b);

					if (min <= 0 || max > current_matrix.size()){
						cout<<"\nВершины не найдены в графе\n"<<endl;
						continue;
					}
				} else {
					break;
				}
			} else if (source == "exit"){
				break;
			} else {
				cout<<"\nInvalid source"<<endl;
				continue;
			}
			
			auto current_matrix = parse_matrix(message.c_str());
			replace(data.begin(), data.end(), ' ', '|');
			string input = message + "|" + data;

			if (!send_udp(sockfd, input)){break;}

			memset(buffer, 0, sizeof(buffer));
			/*
			ssize_t bytes_received = recvfrom(sockfd, buffer, sizeof(buffer), 0, (sockaddr*)&server_address, &addr_len);
			if (bytes_received <= 0){
				cout<<"error to receive"<<endl;
				break;
			} else {
				cout<<"\n"<< buffer <<endl;
			}i
			*/

			if (!receive_udp(sockfd, buffer, sizeof(buffer))){break;}
			cout<<"\n"<< buffer <<endl;
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
	
	bool send_udp(int sockfd, const string& message){
		char ask_buffer[10];
		socklen_t addr_len = sizeof(server_address);
		for (int attempt = 0; attempt < MAX_RETRIES; attempt++){
			ssize_t bytes_sent = sendto(sockfd, message.c_str(), message.length(), 0, (sockaddr*)&server_address, addr_len);
			
			if (bytes_sent <= 0){
				cout<<"Ошибка отправки сообщения (попытка "<< (attempt+1) << ")"<<endl;
				continue;
			}

			struct timeval timeout;
			timeout.tv_sec = TIMEOUT_SEC;
			timeout.tv_usec = 0;
			setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));

			memset(ask_buffer, 0, sizeof(ask_buffer));

			ssize_t ask_received = recvfrom(sockfd, ask_buffer, sizeof(ask_buffer), 0, (sockaddr*)&server_address, &addr_len);

			timeout.tv_sec = 5;
			timeout.tv_usec = 0;
			setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));

			if (ask_received > 0 && string(ask_buffer) == "ASK"){return true;}

			if (attempt < MAX_RETRIES -1){
				cout<<"Повторная отправка (попытка "<< (attempt+1) <<")"<<endl;
			}
		}

		cout<<"Потеряна связь с сервером"<<endl;
		return false;
	}

	bool receive_udp(int sockfd, char* buffer, size_t buffer_size){
		socklen_t addr_len = sizeof(server_address);
		ssize_t bytes_received = recvfrom(sockfd, buffer, buffer_size, 0, (sockaddr*)&server_address, &addr_len);

		if (bytes_received > 0){
			string ask = "ASK";
			sendto(sockfd, ask.c_str(), ask.length(), 0, (sockaddr*)&server_address, addr_len);
			return true;
		}

		return false;
	}


	bool check_connection(){
		char buffer[10];
		socklen_t addr_len = sizeof(server_address);

		ssize_t bytes_received = recvfrom(sockfd, buffer, sizeof(buffer), 0, (sockaddr*)&server_address, &addr_len);

		if (buffer != "1"){return false;}

		return true;
	}
};

int main(int argc, char* argv[]){
	if (argc != 4){
		cout<<"Use: "<<argv[0]<<" <server_ip> <port> <tcp|udp>"<<endl;
		return 1;
	}

	string server_ip = argv[1];
	int port = stoi(argv[2]);
	string protocol = argv[3];
	bool use_tcp = (protocol == "tcp");

	if (!use_tcp && protocol != "udp"){
		cout<<"Incorrect protocol"<<endl;
		return 1;
	}

	Client client(server_ip, port, use_tcp);

	if (!client.connect_to_server()){return 1;}

	client.run();
	return 0;
}
