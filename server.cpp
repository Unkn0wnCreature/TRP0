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
#include <sys/select.h>
#include "matrix.h"
#include "graph.h"
using namespace std;

class Server {
private:
	int server_fd;
	int port;
	bool use_tcp;
	vector<thread> threads; // потоки
	mutex console_mutex; // блокировка для 
	mutex clients_mutex;
	mutex ack_mutex;

	map<int, bool> ack_flags; // таблица для UDP для обработки ACK пакетов
	map<string, bool> active_clients; // таблица активных клиентов

	int client_counter = 0;
	mutex counter_mutex;

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

		// завершаем потоки
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
		//создание сокета
		server_fd = socket(AF_INET, SOCK_STREAM, 0);
		if (server_fd == -1){
			cerr << "Ошибка создания TCP сокета" <<std::endl;
			return false;
		}

		// настройка опции для возможности избежать TIME_WAIT
		int opt = 1;
		if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt))){
			cerr<<"Ошибка установки настроек сокета"<<endl;
			close(server_fd);
			return false;
		}

		// настройка параметров адреса сервера
		sockaddr_in address;
		memset(&address, 0, sizeof(address));
		address.sin_family = AF_INET;
		address.sin_addr.s_addr = INADDR_ANY;
		address.sin_port = htons(port);

		// привязка сокета к порту
		if (bind(server_fd, (sockaddr*) &address, sizeof(address)) < 0){
			cerr<<"Ошибка привязки TCP сокета"<<std::endl;
			close(server_fd);
			return false;
		}

		// перевод в пежим прослушивания входящих подключений
		if (listen(server_fd, 5) < 0){
			cerr<<"Ошибка прослушивания TCP"<<std::endl;
			close(server_fd);
			return false;
		}
		return handle_tcp_connections(server_fd);
	}

	bool start_udp(){
		// создание сокета
		server_fd = socket(AF_INET, SOCK_DGRAM, 0);
		if (server_fd == -1){
			cerr<<"Ошибка создания UDP сокета"<<std::endl;
			return false;
		}

		// настройка параметров адреса сервера
		sockaddr_in address;
		memset(&address, 0, sizeof(address));
		address.sin_family = AF_INET;
		address.sin_addr.s_addr = INADDR_ANY;
		address.sin_port = htons(port);

		// привязка сокета к порту и адресу
		if (bind(server_fd, (sockaddr*) &address, sizeof(address)) < 0){
			cerr<<"Ошибка привязки UDP сокета"<<std::endl;
			close(server_fd);
			return false;
		}
		return handle_udp_connections(server_fd);
	}

	bool handle_tcp_connections(int server_fd){
		sockaddr_in client_address;
		socklen_t addr_len = sizeof(client_address);

		while (true){
			// принимаем подключение
			int client_socket = accept(server_fd, (sockaddr*)&client_address, &addr_len);
			if (client_socket < 0){
				cerr<<"Ошибка принятия клиента"<<std::endl;
				continue;
			}

			char client_ip[INET_ADDRSTRLEN];
			inet_ntop(AF_INET, &client_address.sin_addr, client_ip, INET_ADDRSTRLEN);

			//увеличиваем счётчик клиентов
			int current_client_id;
			{
				lock_guard<mutex> lock(console_mutex);
				current_client_id = ++client_counter;
			}

			// запускаем новый поток обработки клиента
			threads.emplace_back([this, client_socket, current_client_id, client_address]() {
					handle_tcp_client(client_socket);
				});
			
				// очистка завершённых потоков
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
		char buffer[25000];
		sockaddr_in client_address;
		socklen_t addr_len = sizeof(client_address);
		ssize_t bytes_sent, bytes_received;

		while (true){
			memset(buffer, 0, sizeof(buffer));

			// получаем сообщение от клиента
			bytes_received = recvfrom(server_fd, buffer, sizeof(buffer), 0, (sockaddr*)&client_address, &addr_len);

			if (bytes_received <= 0){
				continue;
			}
			
			// копируем текст сообщения
			string received = buffer;

			// если ACK от потока -> меняем значение в таблице
			if (received.find("ACK_ID=") == 0){
				int ack_id = stoi(received.substr(7));
				{
					lock_guard<mutex> lock(ack_mutex);
					ack_flags[ack_id] = true;
				}
				continue;
			}

			// копируем тест сообщения клиента для передачи в поток
			char data_copy[25000];
			strcpy(data_copy, buffer);

			// создание потока обработки запроса
			threads.emplace_back([this, server_fd, data_copy, bytes_received, client_address](){
					handle_udp_client(server_fd, data_copy, bytes_received, client_address);
					});

					//очистка завершённых потоков
			if (threads.size() > 100){
				threads.erase(remove_if(threads.begin(), threads.end(),
							[](const thread& t){
							return !t.joinable();
							}),
						threads.end());
			}
		}
		return true;
	}

	void handle_tcp_client(int client_socket){
		char buffer[25000];

		while (true){
			memset(buffer, 0, sizeof(buffer));
			
			// получаем от клента пакет данных
			ssize_t bytes_received = recv(client_socket, buffer, 25000, 0);

			// проверка на ошибку получения данных
			if (bytes_received <= 0){
				break;
			}

			// читаем данные из пакета
			auto [matr, dot] = read_data(buffer);
			replace(dot.begin(), dot.end(), '|', ' ');

			// создаём строку ответа
			string response;

			// блок валидации
			if (!isValidMatrix(matr.c_str())){
				response = "Некорректный формат ввода графа";	
			} else if (!matrix_is_correct(matr.c_str(), 6)){
				response = "Некорректная матрица смежности";	
			} else {
				auto matrix = parse_matrix(matr.c_str());

				auto [a, b] = get_elements(dot.c_str());
				
				int min = (a <= b ? a : b);
				int max = (a <= b ? b : a);

				if (min <= 0 || max > matrix.size()){
					response = "Вершины не найдены в графе";
				} else {
					auto [min_dist, path] = dijkstra(matrix, a-1, b-1);

					if (min_dist == INF){
						response = "Пути между вершинами не существует";
					} else{
						response = "Результат: " + convert_len_to_string(min_dist) + "\nКратчайший путь: " + convert_path_to_string(path);
					}
				}
			}

			memset(buffer, 0, sizeof(buffer));

			// отправка клиенту результата
			if (!send_tcp(client_socket, response)){
				cout<<"Ошибка отправки данных"<<endl;
				break;
			}
		}
	}

	void handle_udp_client(int server_fd, const char* buffer_data, ssize_t data_size, sockaddr_in client_address){
		char ack_buffer[32];
		ssize_t bytes_sent;
		ssize_t bytes_received;

		// копирование текста сообщения буффера
		string data = buffer_data;

		// получаем id ыщщбщения из строки запроса
		size_t id_pos = data.find("|MSG_ID=");
		int msg_id = stoi(data.substr(id_pos + 8));
		data = data.substr(0, id_pos);
		char m_data[25000];
		strncpy(m_data, data.c_str(), sizeof(m_data));

		// копируем адрес клиента
		sockaddr_in target_client = client_address;
		socklen_t target_len = sizeof(target_client);

		// создание временного адресса
		sockaddr_in temp_addr;
		socklen_t temp_len = sizeof(temp_addr);
		
		// отправка подтверждения получения
		string ack = "ACK_ID=" + to_string(msg_id);
		bytes_sent = sendto(server_fd, ack.c_str(), ack.length(), 0, (sockaddr*)&target_client, target_len);

		auto [matr, dot] = read_data(m_data);
		replace(dot.begin(), dot.end(), '|', ' ');

		// создаём строку ответа
		string result;

		// блок валидации
		if (!isValidMatrix(matr.c_str())){
			result = "Неверный формат ввода графа";
		} else if (!matrix_is_correct(matr.c_str(), 6)){
			result = "Некорректная матрица смежности";
		} else {
			auto matrix = parse_matrix(matr.c_str());

			auto [a, b] = get_elements(dot.c_str());

			int min = (a <= b ? a : b);
			int max = (a <= b ? b : a);
			
			if (min <= 0 || max > matrix.size()){
				result = "Вершины не найдены в графе";
			} else {
				auto [min_dist, path] = dijkstra(matrix, a-1, b-1);

				if (min_dist == INF){
					result = "Пути между вершинами не существует";
				} else{
					result = "Результат: " + convert_len_to_string(min_dist) + "\nКратчайший путь: " + convert_path_to_string(path);
				}
			}
		}

		// формируем строку ответа
		string response = "RESP_ID=" + to_string(msg_id) + "|" + result;

		// отправка с подтверждением (3 попытки)
		bool result_sent = false;
		for (int attempt = 1; attempt <= 3; attempt++){
			bytes_sent = sendto(server_fd, response.c_str(), response.length(), 0, (sockaddr*)&target_client, target_len);
			
			// если не отправилось
			if (bytes_sent <= 0){
				cout<<"Error to sent (attempt "<< (attempt)<<")"<<endl;
				continue;
			}
				
			// проверка элемента таблицы ack_id (значение меняется в главном потоке)
			for (int wait = 0; wait < 3000; wait++){
				{
					lock_guard<mutex> lock(ack_mutex);
					if (ack_flags[msg_id]){
						result_sent = true;
						break;
					}
				}
			}

			// успешно отправлено
			if (result_sent){break;}
		}

		if (!result_sent){
			cout<<"Клиент отключился"<<endl;
		}

		// меняем значение в таблице ack_id
		{
			lock_guard<mutex> lock(ack_mutex);
			ack_flags.erase(msg_id);
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
};
