/*
 * main.cpp
 *
 *  Created on: Mar 18, 2018
 *      Author: Conner Ebbinghaus
 */

#include <iostream>
#include <vector>
#include "TBAApi.hpp"
#include "doublefann.h"
#include "fann_cpp.h"

int main() {
	FANN::neural_net nnet;
	nnet.create_standard(4, 24, 30, 12, 2);


	auto eventKeys = TBAApi("/events/2018/keys").get();
	for(const auto& eventKey : eventKeys)
	{
		std::cout << "Downloading data for event" << eventKey << "..." << std::endl;
		auto matchKeys = TBAApi(std::string("/event/") + eventKey.get<std::string>() + std::string("/matches/keys")).get();
		std::vector<std::pair<double*, double*> > data;
		for(const auto& matchKey : matchKeys)
		{
			const auto match = TBAApi("/match/" + matchKey.get<std::string>()).get();
			if(!match["winning_alliance"].get<std::string>().empty())
			{
				try {
					double* input = new double[24];
					double* output = new double[2];
					unsigned int i = 0;
					for(const auto& team : match["alliances"]["blue"]["team_keys"])
					{
						auto teamInfo = TBAApi("/team/" + team.get<std::string>() + "/event/" + eventKey.get<std::string>() + "/status").get();
						input[i] = teamInfo["qual"]["ranking"]["record"]["wins"];
						input[i+1] = teamInfo["qual"]["ranking"]["record"]["ties"];
						input[i+2] = teamInfo["qual"]["ranking"]["record"]["losses"];
						input[i+3] = teamInfo["qual"]["ranking"]["rank"];
						i+=4;
					}
					for(const auto& team : match["alliances"]["red"]["team_keys"])
					{
						auto teamInfo = TBAApi("/team/" + team.get<std::string>() + "/event/" + eventKey.get<std::string>() + "/status").get();
						input[i] = teamInfo["qual"]["ranking"]["record"]["wins"];
						input[i+1] = teamInfo["qual"]["ranking"]["record"]["ties"];
						input[i+2] = teamInfo["qual"]["ranking"]["record"]["losses"];
						input[i+3] = teamInfo["qual"]["ranking"]["rank"];
						i+=4;
					}

					output[0] = match["alliances"]["blue"]["score"];
					output[1] = match["alliances"]["red"]["score"];
					std::cout << "#";
					std::cout.flush();
					data.push_back(std::make_pair(input, output));
				} catch (...) {
					std::cout << "X";
					std::cout.flush();
				}
			}
		}
		std::cout << std::endl << "Training..." << std::endl;
		FANN::training_data tData;
		double* inputs[data.size()];
		double* outputs[data.size()];
		unsigned int i=0;
		for(auto d : data)
		{
			inputs[i] = d.first;
			outputs[i] = d.second;
			i++;
		}
		tData.set_train_data(data.size(), 24, (double**)inputs, 2, (double**)outputs);
		nnet.train_epoch(tData);
		for(unsigned int j = 0; j < data.size(); j++)
		{
			delete[] inputs[j];
			delete[] outputs[j];
		}
	}
	nnet.save("nnet.out");
}
