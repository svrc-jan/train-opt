#pragma once

#include <nlohmann/json.hpp>

using json = nlohmann::json;

json get_json_from_file(const std::string& file_name);

template<typename X>
void json_update(X& x, const std::string& field_name, const json& jsn)
{
	if (jsn.contains(field_name)) {
		x = jsn[field_name];
	}
}
