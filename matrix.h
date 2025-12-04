#include <string>
#include <iostream>
#include <vector>
#include <sstream>
#include <regex>
#include <cstring>
using namespace std;

int sum_matrix(const vector<vector<int>> matrix){
	int sum = 0;

	for (const auto& row : matrix){
		for (int num : row){
			if (num > 0){sum++;}
		}
	}
	return sum;
}


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

bool matrix_is_correct(const char* strmatrix, size_t amount){
	auto matrix = parse_matrix(strmatrix);

	string temp = strmatrix;
	if (temp == "exit"){return true;}

	if (matrix.size() < amount){return false;}

	for (const auto& row : matrix){
		if (row.size() != matrix.size()){return false;}
	}

	if (sum_matrix(matrix) < 2*amount){return false;}

	return true;
}

bool isValidMatrix(const char* str){
	if (str == "exit"){return true;}
	string cleaned = str;
	cleaned.erase(remove(cleaned.begin(), cleaned.end(), ' '), cleaned.end());

	regex pattern(R"(^\[(\[(\d+,)*\d+\],)*\[(\d+,)*\d+\]\]$)");
	if (!regex_match(cleaned, pattern)){return false;}

	auto test_matrix = parse_matrix((cleaned.c_str()));

	size_t row_size = (*test_matrix.begin()).size();

	for (const auto& row : test_matrix){
		if (row.size() != row_size){return false;}
	}

	for (int i = 0; i < test_matrix.size(); i++){
		for (int j = 0; j < test_matrix.size(); j++){
			if (test_matrix[i][j] != 0 && test_matrix[i][j] != 1){return false;}
			if (test_matrix[i][j] != test_matrix[j][i]){return false;} 
		}
	}

	return true;
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

