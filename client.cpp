#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <iostream>
#include <cstring>
#include <string>
#include "matrix.h"
#include "graph.h"
using namespace std;


class Client {
private:
	int sockfd;
	bool use_tcp;
	string server_ip;
	int port;
	sockaddr_in server_address;
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

		if (connect(sockfd, (sockaddr*)&server_address, sizeof(server_address)) < 0){
			cerr<<"\nНе удалось подключиться к серверу"<<endl;
			close(sockfd);
			sockfd = -1;
			return false;
		}

		cout<<"Connected to TCP server"<<endl;
		return true;
	}

	bool connect_udp(){
		sockfd = socket(AF_INET, SOCK_DGRAM, 0);
		if (sockfd == -1){
			cerr<<"Error to create UDP socket"<<endl;
			return false;
		}

		cout<<"UDP ready to send data on "<< server_ip <<":"<<port<<endl;
		return true;
	}

	void run_tcp(){
		string message;
		char buffer[1024];
		ssize_t bytes_received = recv(sockfd, buffer, sizeof(buffer), 0);
			if (bytes_received > 0){
				cout<< buffer <<endl;
			}

		while (true){
			cout<<"\nВведите описание графа: "<<endl;
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
			}
			
			auto current_matrix = parse_matrix(message.c_str());

			ssize_t bytes_sent = send(sockfd, message.c_str(), message.length(), 0);

			if (bytes_sent < 0){
				cerr<<"Error to send message"<<endl;
				break;
			}

			if (message == "exit"){break;}

			/*
			memset(buffer, 0, sizeof(buffer));
			ssize_t bytes_received = recv(sockfd, buffer, sizeof(buffer), 0);
			if (bytes_received > 0){
				cout<<"Server replyed: " << buffer <<endl;
			} else {
				cerr<<"Server disconnected"<<endl;
				break;
			}
			*/
			//-------------------------------------------------------------------
			cout<<"\nВведите начальную и конечную вершины:"<<endl;
			getline(cin, message);

			if (message.empty()){
				continue;
			}
			
			if (message != "exit"){
				auto [a, b] = get_elements(message.c_str());
				int max = (a<=b ? b : a);
				int min = (a<=b ? a : b);

				if (min < 0 || max >= current_matrix.size()){
					cout<<"\nВершины не найдены в графе\n"<<endl;
					continue;
				}
			}

			bytes_sent = send(sockfd, message.c_str(), message.length(), 0);

			if (bytes_sent < 0){
				cerr<<"Error to send message"<<endl;
				break;
			}

			if (message == "exit"){break;}
			
			memset(buffer, 0, sizeof(buffer));
			bytes_received = recv(sockfd, buffer, sizeof(buffer), 0);
			if (bytes_received > 0){
				cout<<"Server replyed: " << buffer <<endl;
			} else {
				cerr<<"Server disconnected"<<endl;
				break;
			}

		}	
	}

	void run_udp(){
		string message;
		char buffer[1024];
		socklen_t addr_len = sizeof(server_address);

		while (true){
			cout<<"Enter message (or exit):"<<endl;
			getline(cin, message);

			if (message.empty()){continue;}
			
			if (message != "exit"){

				if (!isValidMatrix(message.c_str())){
					cout<<"Invalid matrix"<<endl;
					continue;
				}

				if (!matrix_is_correct(message.c_str(), 2)){
					cout<<"Matrix must contain at least 6 points"<<endl;
					continue;
				}
			}

			ssize_t bytes_sent = sendto(sockfd, message.c_str(), message.length(), 0, (sockaddr*)&server_address, addr_len);

			if (bytes_sent < 0){
				cerr<<"Error to sent"<<endl;
				break;
			}

			if (message == "exit"){break;}

			memset(buffer, 0, sizeof(buffer));
			ssize_t bytes_received = recvfrom(sockfd, buffer, sizeof(buffer), 0, (sockaddr*)&server_address, &addr_len);

			if (bytes_received > 0){
				cout<<"Server replied: "<< buffer <<endl;
			} else {
				cerr<<"Error to receive message"<<endl;
			}
		}
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
