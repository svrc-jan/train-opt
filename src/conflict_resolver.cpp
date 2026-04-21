#include "conflict_resolver.hpp"


Conflict_resolver::Conflict_resolver(Solver& solver)
	: inst(solver.inst), prepr(solver.prepr), slvr(solver), model(solver.grb_env)
{

};


Conflict_resolver::~Conflict_resolver()
{

}


void Conflict_resolver::init_data()
{
	
}
