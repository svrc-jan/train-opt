#pragma once

#include "gurobi_c++.h"

#include "instance.hpp"
#include "preprocess.hpp"

class Route_planner
{
public:
	const Instance& inst;
	const Preprocess& prepr;

	Route_planner(const Preprocess& prepr, GRBEnv& grb_env);
	~Route_planner();

private:
	GRBEnv& grb_env;
	GRBModel model;
};
