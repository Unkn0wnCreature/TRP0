#include <string>
#include <iostream>
#include <vector>
#include <sstream>
using namespace std;

vector<vector<int>> parse_matrix(const char*  matrix_str){
	vector<vector<int>> matrix;
	string matrix_string = matrix_str;
	stringstream ss(matrix_string);
	char ch;

	ss>>ch;

	while (ss>>ch && ch != ']'){
		if (ch == '['){
			vector<int> row;
			int num;

			while (ss>>num){
				row.push_back(num);
				ss>>ch;
				if (ch ==']'){break;}
			}
			matrix.push_back(row);
		}
	}
	return matrix;
}

void show_matrix(const char* strmatrix){
	auto matrix = parse_matrix(strmatrix);

	for (const auto& row : matrix){
		for (int num :row){
			cout<<num<<" ";
		}
		cout<<endl;
	}
}

