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
		timeout.tv_sec = 10;
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

		//struct timeval timeout;
		//timeout.tv_sec = 3;
		//timeout.tv_usec = 0;

		//if (setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout)) < 0){
			//cout<<"Ошибка присвоения таймаута на получение"<<endl;
			//return false;
		//}

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
		ssize_t bytes_sent, bytes_received;
		
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

			//send_udp(sockfd, input);

			bool data_sent = false;
			for (int attempt = 1; attempt <= 3; attempt++)	{
				bytes_sent = sendto(sockfd, input.c_str(), input.length(), 0, (sockaddr*)&server_address, addr_len);
				cout<<"Client sent: "<<input<<endl;

				if (bytes_sent <= 0){continue;}

				memset(buffer, 0, sizeof(buffer));

				bytes_received = recvfrom(sockfd, buffer, sizeof(buffer), 0, (sockaddr*)&server_address, &addr_len);
				
				if (bytes_received > 0 && string(buffer) == "ACK"){
					cout<<"Client received (ACK): "<<buffer<<endl;
					data_sent = true;
					break;
				} else {
					cout<<"ACK not received"<<endl;
				}
			}

			if (!data_sent){
				cout<<"Unable to sent data on server"<<endl;
				continue;
			}

			memset(buffer, 0, sizeof(buffer));

			bytes_received = recvfrom(sockfd, buffer, sizeof(buffer), 0, (sockaddr*)&server_address, &addr_len);
			if (bytes_received > 0){
				cout<<"Client received: "<<buffer<<endl;
				string ack = "ACK";
				sleep(1);	
				bytes_sent = sendto(sockfd, ack.c_str(), ack.length(), 0, (sockaddr*)&server_address, addr_len);
				cout<<"Client sent: "<<ack<<endl;
				cout<<"\n"<< buffer <<endl;
				//memset(buffer, 0, sizeof(buffer));
			} else {
				cout<<"unavle to receive result from server"<<endl;
			}	
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
