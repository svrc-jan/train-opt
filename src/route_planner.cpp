#include "route_planner.hpp"


Route_planner::Route_planner(const Preprocess& prepr, GRBEnv& grb_env)
	: inst(prepr.inst), prepr(prepr), grb_env(grb_env), model(GRBModel(grb_env))
{
	
}