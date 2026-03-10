#include "files.hpp"

bool file_exists(const std::string& name) {
	if (FILE *file = fopen(name.c_str(), "r")) {
		fclose(file);
		return true;
	} else {
		return false;
	}   
}


std::string get_parent_dir(const std::string& s)
{	
	if (s.rfind("/", 0) == 0) {
		return s;
	}

	if (s.rfind("./", 0) == 0) {
		return "." + s; 
	}

	return "../" + s;
}

json get_json_file(const std::string& file_name)
{
	std::string file_name_parent = get_parent_dir(file_name);
	std::ifstream file;

	if (file_exists(file_name)) {
		// std::cerr << "json file '" << file_name << "' found" << std::endl;
		file.open(file_name);
	}
		
	else {
		std::cerr << "json file '" << file_name << "' not found" << std::endl;
		exit(1);
	}

	json config = json::parse(file);

	return config;
}