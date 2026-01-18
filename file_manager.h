#include <iostream>
#include <cstring>
#include <string>
#include <fstream>

using namespace std;

pair<string, string> read_from_file(const string& filename){
	ifstream file(filename);
	if (!file.is_open()){
		cout<<"Unable to open file"<<endl;
		return {"", ""};
	}
	
	string matrix_data;
	string dot;

	getline(file, matrix_data);
	getline(file, dot);

	return {matrix_data, dot};
}
