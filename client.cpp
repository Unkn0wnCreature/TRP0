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

		struct timeval timeout;
		timeout.tv_sec = 3;
		timeout.tv_usec = 0;

		if (setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout)) < 0){
			cout<<"Ошибка присвоения таймаута на получение"<<endl;
			return false;
		}

		return true;
	}

	void run_tcp(){
		string message, data, source, filename, s_size, row;
		int size;
		char buffer[1024];

		while (true){
			cout<<"Источник данных (1 - файл; 2 - клавиатура):"<<endl;
			getline(cin, source);

			if (source == "exit"){break;}

			if (source == "1"){
				cout<<"Введите путь к файлу:"<<endl;
				getline(cin, filename);

				if (filename.empty()){continue;}

				auto [m, d] = read_from_file(filename);

				message = m;
				data = d;

				if (message.empty() || data.empty()){continue;}
			} else if (source == "2"){
				cout<<"Введите количество вершин:"<<endl;
				getline(cin, s_size);

				if (s_size == "exit"){break;}

				size = stoi(s_size);

				for (int i = 0; i < size; i++){
					cout<<"Введите ряд "<< i + 1 <<":"<<endl;
					getline(cin, row);
					replace(row.begin(), row.end(), ' ', ',');
					row = "[" + row + "]";
					if (i == 0){
						message = row;
					} else {
						message = message + "," + row;
					}
				}

				message = "[" + message + "]";

				cout<<"Введите начальную и конечную вершины:"<<endl;
				getline(cin, data);

				if (data == "exit"){break;}

				if (data.empty()){continue;}
			} else {continue;}
			
			auto current_matrix = parse_matrix(message.c_str());
			replace(data.begin(), data.end(), ' ', '|');
			string input = message + "|" + data;

			if (!send_tcp(sockfd, input)){
				cout<<"Ошибка отправки"<<endl;
				break;
			}
			//-------------------------------------------------------------------
			memset(buffer, 0, sizeof(buffer));

			if (!receive_tcp(sockfd, buffer)){
				cout<<"Потеряна связь с сервером"<<endl;
				break;
			}
			cout<<"\n"<< buffer <<"\n"<<endl;
		}

	}

	void run_udp(){
		static int message_id = 0;
		int size;
		string message, data, source, filename, s_size, row;
		char buffer[1024];
		socklen_t addr_len = sizeof(server_address);
		ssize_t bytes_sent, bytes_received;
		
		while (true){
			cout<<"Источник данных (1 - файл; 2 - клавиатура):"<<endl;
			getline(cin, source);

			if (source == "exit"){break;}

			if (source == "1"){
				cout<<"Введите путь к файлу:"<<endl;
				getline(cin, filename);

				if (filename.empty()){continue;}

				auto [m, d] = read_from_file(filename);

				message = m;
				data = d;

				if (message.empty() || data.empty()){continue;}
			} else if (source == "2"){
				cout<<"Введите количество вершин:"<<endl;
				getline(cin, s_size);

				if (s_size == "exit"){break;}

				size = stoi(s_size);

				for (int i = 0; i < size; i++){
					cout<<"\nВведите ряд "<< i + 1 <<":"<<endl;
					getline(cin, row);
					replace(row.begin(), row.end(), ' ', ',');
					row = "[" + row + "]";
					if (i == 0){
						message = row;
					} else {
						message = message + "," + row;
					}
				}

				message = "[" + message + "]";

				cout<<"Введите начальную и конечную вершины:"<<endl;
				getline(cin, data);

				if (data == "exit"){break;}

				if (data.empty()){continue;}
			} else {continue;}
			
			auto current_matrix = parse_matrix(message.c_str());
			replace(data.begin(), data.end(), ' ', '|');
			string input = message + "|" + data + "|MSG_ID=" + to_string(++message_id);

			bool data_sent = false;
			for (int attempt = 1; attempt <= 3; attempt++)	{
				bytes_sent = sendto(sockfd, input.c_str(), input.length(), 0, (sockaddr*)&server_address, addr_len);
				//cout<<"Client sent: "<<input<<endl;

				if (bytes_sent <= 0){continue;}

				memset(buffer, 0, sizeof(buffer));

				bytes_received = recvfrom(sockfd, buffer, sizeof(buffer), 0, (sockaddr*)&server_address, &addr_len);

				string received = buffer;
				
				if (bytes_received > 0 && received.find("ACK_ID=" + to_string(message_id)) == 0){
					//cout<<"Client received (ACK): "<<received<<endl;
					data_sent = true;
					break;
				} else {
					//cout<<"ACK not received"<<endl;
				}
			}

			if (!data_sent){
				cout<<"Ошибка отправки"<<endl;
				break;
			}

			memset(buffer, 0, sizeof(buffer));

			bytes_received = recvfrom(sockfd, buffer, sizeof(buffer), 0, (sockaddr*)&server_address, &addr_len);
			string result = buffer;
			if (bytes_received > 0 && result.find("RESP_ID=" + to_string(message_id)) == 0){
				//cout<<"Client received (result): "<<result<<endl;
				string ack = "ACK_ID=" + to_string(message_id);
				bytes_sent = sendto(sockfd, ack.c_str(), ack.length(), 0, (sockaddr*)&server_address, addr_len);
				//cout<<"Client sent (ACK): "<<ack<<endl;
				cout<< "\n" << result.substr(result.find("|") + 1) <<"\n"<<endl;
			} else {
				cout<<"Ошибка получения данных от сервера"<<endl;
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
		cout<<"Некорректный протокол"<<endl;
		return 1;
	}

	Client client(server_ip, port, use_tcp);

	if (!client.connect_to_server()){return 1;}

	client.run();
	return 0;
}
