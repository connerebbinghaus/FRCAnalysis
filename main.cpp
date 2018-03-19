/*
 * main.cpp
 *
 *  Created on: Mar 18, 2018
 *      Author: Conner Ebbinghaus
 */

#include <iostream>
#include "TBAApi.hpp"
#include "fann_cpp.h"

int main() {
	FANN::neural_net nnet;
	nnet.create_standard(4, 24, 30, 12, 1);


	auto eventKeys = TBAApi("/eventKeys/2018/keys").get();
	for(const auto& eventKey : eventKeys)
	{
		auto matchKeys = TBAApi(std::string("/eventKey/") + eventKey.get<std::string>() + std::string("/matches/keys")).get();
		for(const auto& matchKey : matchKeys)
		{
			const auto match = TBAApi("/match/" + matchKey.get<std::string>()).get();
			if(!match["winning_alliance"].get<std::string>().empty())
			{

			}
		}
	}
}
