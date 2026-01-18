#include <iostream>
#include <vector>
#include <queue>
#include <limits>
#include <algorithm>
#include <sstream>
#include <string>
#include <cstring>

using namespace std;

const int INF = numeric_limits<int>::max();

string convert_len_to_string(int length){
	return to_string(length);
}

string convert_path_to_string(vector<int> path){
	string converted_path;
	for (int i = 0; i < path.size(); i++){
		converted_path += to_string(path[i]+1);
		if (i != path.size() - 1){converted_path += " -> ";}
	}
	return converted_path;
}

pair<int, vector<int>> dijkstra(const vector<vector<int>>& matrix, int start, int end){
	int n = matrix.size();
	vector<int> distance(n, INF);
	vector<int> previous(n, -1);

	priority_queue<pair<int, int>, vector<pair<int, int>>, greater<pair<int, int>>> pq;
	distance[start] = 0;
	pq.push({0, start});

	while (!pq.empty()){
		int current_dist = pq.top().first;
		int current_vertex = pq.top().second;
		pq.pop();

		if (current_dist > distance[current_vertex]){continue;}

		if (current_vertex == end){break;}

		for (int neighbor = 0; neighbor < n; neighbor++){
			if (matrix[current_vertex][neighbor] != 0){
				int edge_weight = 1;
				int new_dist = distance[current_vertex] + edge_weight;

				if (new_dist < distance[neighbor]){
					distance[neighbor] = new_dist;
					previous[neighbor] = current_vertex;
					pq.push({new_dist, neighbor});
				}
			}
		}
	}

	vector<int> path;
	if (distance[end] != INF){
		int current = end;
		while (current != -1){
			path.push_back(current);
			current = previous[current];
		}
		reverse(path.begin(), path.end());
	}
	return {distance[end], path};
}

pair<int, int> get_elements(const char* str){
	int a,b;
	string str_elements = str;
	stringstream ss(str_elements);
	//char ch;

	ss>>a;
	//ss>>ch;
	ss>>b;

	return {a,b};
}

pair<string, string> read_data(char* str){
	string stri = str;
	stringstream ss(stri);

	string  matr, dot;
	char c;

	while (ss>>c && c != '|'){
		matr = matr + c;
	}

	while (ss>>c){
		dot = dot + c;
	}

	//const char* m = matr.c_str();
	//const char* d = dot.c_str();

	return {matr, dot};
}
