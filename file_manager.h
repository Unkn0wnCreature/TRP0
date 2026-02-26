#include <iostream>
#include <cstring>
#include <string>
#include <fstream>
#include <algorithm>
#include <sstream>

using namespace std;

// чтение данных из файла
pair<string, string> read_from_file(const string& filename){
	ifstream file(filename);
	if (!file.is_open()){
		cout<<"Unable to open file"<<endl;
		return {"", ""};
	}

	string s_size;
	string data;
	string matrix;
	string dot;
	string row;
	int size;
	char separator, ch;

	// получаем размер матрицы и начало-конец
	getline(file, data);

	stringstream ss(data);
	ss>>size;

	ss>>separator;

	while (ss>>ch){
		dot = dot + ch;
	}

	replace(dot.begin(), dot.end(), '-', ' ');

	// построчно считываем матрицу смежности
	for (int i = 0; i < size; i++){
		getline(file, row);
		replace(row.begin(), row.end(), ' ', ',');
		row = "[" + row + "]";

		if (i == 0){
			matrix = row;
		} else {
			matrix = matrix + "," + row;
		}
	}
	
	// формируем строку описания графа
	matrix = "[" + matrix + "]";

	return {matrix, dot};
}
