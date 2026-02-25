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

	const int TIMEOUT_SEC = 3;
public:
	Client(const string& ip, int port, bool use_tcp){
		sockfd = -1;
		this->server_ip = ip;
		this->port = port;
		this->use_tcp = use_tcp;

		// задаём параметры адреса клиента
		memset(&server_address, 0, sizeof(server_address));
		server_address.sin_family = AF_INET;
		server_address.sin_port = htons(port);
		inet_pton(AF_INET, server_ip.c_str(), &server_address.sin_addr);

	}

	~Client(){
		// деструктор -> закрываем сокет
		if (sockfd != -1){
			close(sockfd);
		}
	}

	bool connect_to_server(){
		// определение сценария подключения к серверу
		if (use_tcp){
			return connect_tcp();
		} else{
			return connect_udp();
		}
	}

	void run(){
		// запуск сценария для определённого протокола
		if (use_tcp){
			run_tcp();
		} else{
			run_udp();
		}
	}

private:
	bool connect_tcp(){
		// создание сокета
		sockfd = socket(AF_INET, SOCK_STREAM, 0);
		if (sockfd == -1){
			cerr<<"Ошибка создания TCP сокета"<<endl;
			return false;
		}

		struct timeval timeout;
		timeout.tv_sec = 10;
		timeout.tv_usec = 0;

		// присваиваем таймаут по истечении которого сокет закрывается
		if (setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout)) < 0){
			cout<<"Ошибка присвоения таймаута на получение"<<endl;
			return false;
		}

		// подключение к серверу
		if (connect(sockfd, (sockaddr*)&server_address, sizeof(server_address)) < 0){
			cerr<<"Не удалось подключиться к серверу"<<endl;
			close(sockfd);
			sockfd = -1;
			return false;
		}

		return true;
	}

	bool connect_udp(){
		// создание сокета
		sockfd = socket(AF_INET, SOCK_DGRAM, 0);
		if (sockfd == -1){
			cerr<<"Ошибка создания UDP сокета"<<endl;
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
		char buffer[25000];

		while (true){
			// выбор способа ввода графа
			cout<<"Источник данных (1 - файл; 2 - клавиатура):"<<endl;
			getline(cin, source);

			// выход по ключевому слову exit
			if (source == "exit"){break;}

			// выбран файл
			if (source == "1"){
				cout<<"Введите путь к файлу:"<<endl;
				getline(cin, filename);

				if (filename.empty()){continue;}

				auto [m, d] = read_from_file(filename);

				message = m;
				data = d;

				if (message.empty() || data.empty()){continue;}
			} else if (source == "2"){
				// выбран ввод с клавиатуры
				// ввод количества вершин
				cout<<"Введите количество вершин:"<<endl;
				getline(cin, s_size);

				// выход по ключевому слову
				if (s_size == "exit"){break;}

				// преобразование строки в число
				size = stoi(s_size);

				// построчный ввод матрицы смежности
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

				// формирование строки описания графа
				message = "[" + message + "]";

				// ввод начала - конца
				cout<<"Введите начальную и конечную вершины:"<<endl;
				getline(cin, data);

				// выход по ключевому слову
				if (data == "exit"){break;}

				if (data.empty()){continue;}
			} else {continue;}
			
			//auto current_matrix = parse_matrix(message.c_str());
			// формирование пакета данных серверу
			replace(data.begin(), data.end(), ' ', '|');
			string input = message + "|" + data;

			// отправка пакета данных серверу
			if (!send_tcp(sockfd, input)){
				cout<<"Ошибка отправки"<<endl;
				break;
			}

			memset(buffer, 0, sizeof(buffer));

			// получение ответа от сервера
			if (!receive_tcp(sockfd, buffer)){
				cout<<"Потеряна связь с сервером"<<endl;
				break;
			}
			// вывод ответа от сервера
			cout<<"\n"<< buffer <<"\n"<<endl;
		}

	}

	void run_udp(){
		static int message_id = 0; // номер обращения клиента
		int size;
		string message, data, source, filename, s_size, row;
		char buffer[25000];
		socklen_t addr_len = sizeof(server_address);
		ssize_t bytes_sent, bytes_received;
		
		while (true){
			// выбор способа ввода данных
			cout<<"Источник данных (1 - файл; 2 - клавиатура):"<<endl;
			getline(cin, source);

			//выход по ключевому слову
			if (source == "exit"){break;}

			// чтение из файла
			if (source == "1"){
				cout<<"Введите путь к файлу:"<<endl;
				getline(cin, filename);

				if (filename.empty()){continue;}

				// чтение данных из файла filename
				auto [m, d] = read_from_file(filename);

				message = m;
				data = d;

				if (message.empty() || data.empty()){continue;}
			} else if (source == "2"){
				// выбран ввод с клавиатуры
				// ввод количества вершин
				cout<<"Введите количество вершин:"<<endl;
				getline(cin, s_size);

				// выход по ключевому слову
				if (s_size == "exit"){break;}

				// приведение строки к числу
				size = stoi(s_size);

				// построчный ввод матрицы смежности
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

				// формирование строки опсания графа
				message = "[" + message + "]";

				// ввод начала - конца
				cout<<"Введите начальную и конечную вершины:"<<endl;
				getline(cin, data);

				// выход по ключевому слову
				if (data == "exit"){break;}

				if (data.empty()){continue;}
			} else {continue;}
			
			//auto current_matrix = parse_matrix(message.c_str());
			// формирование пакета данных для отправки на сервер
			replace(data.begin(), data.end(), ' ', '|');
			string input = message + "|" + data + "|MSG_ID=" + to_string(++message_id);

			// отправка данных на сервер (3 попытки)
			bool data_sent = false;
			for (int attempt = 1; attempt <= 3; attempt++)	{
				bytes_sent = sendto(sockfd, input.c_str(), input.length(), 0, (sockaddr*)&server_address, addr_len);

				// пакет не отправлен
				if (bytes_sent <= 0){continue;}

				memset(buffer, 0, sizeof(buffer));

				// получаем ACK от сервера
				bytes_received = recvfrom(sockfd, buffer, sizeof(buffer), 0, (sockaddr*)&server_address, &addr_len);

				// копируем текст сообщения
				string received = buffer;
				
				// проверка на получение ACK-пакета
				if (bytes_received > 0 && received.find("ACK_ID=" + to_string(message_id)) == 0){
					data_sent = true;
					break;
				}
			}

			// если данные не отправлены
			if (!data_sent){
				cout<<"Сервер отключился"<<endl;
				break;
			}

			memset(buffer, 0, sizeof(buffer));

			// ждём результат от сервера
			bytes_received = recvfrom(sockfd, buffer, sizeof(buffer), 0, (sockaddr*)&server_address, &addr_len);
			string result = buffer;
			
			// проверка на получение результата от сервера
			if (bytes_received > 0 && result.find("RESP_ID=" + to_string(message_id)) == 0){
				string ack = "ACK_ID=" + to_string(message_id);
				bytes_sent = sendto(sockfd, ack.c_str(), ack.length(), 0, (sockaddr*)&server_address, addr_len);
				cout<< "\n" << result.substr(result.find("|") + 1) <<"\n"<<endl;
			} else {
				cout<<"Сервер отключился"<<endl;
			}	
		}
	}

	bool send_tcp(int sockfd, const string& message){
		ssize_t bytes_sent = send(sockfd, message.c_str(), message.length(), 0);

		return (bytes_sent > 0);
	}

	bool receive_tcp(int sockfd, char buffer[25000]){
		ssize_t bytes_received = recv(sockfd, buffer, 25000, 0);

		return (bytes_received > 0);
	}
};

// запуск сервера с параметрами командной строки
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
