#include "json_aux.hpp"

#include <string>
#include <iostream>
#include <fstream>

bool file_exists(const std::string& name) {
	if (FILE *file = fopen(name.c_str(), "r")) {
		fclose(file);
		return true;
	} else {
		return false;
	}   
}


json get_json_from_file(const std::string& file_name)
{
	
	if (!file_exists(file_name)) {
		throw std::runtime_error("json file is missing");
	}
	
	std::ifstream file;
	file.open(file_name);
	json jsn = json::parse(file);

	return jsn;
}
