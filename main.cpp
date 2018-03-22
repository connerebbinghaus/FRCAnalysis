/*
 * main.cpp
 *
 *  Created on: Mar 18, 2018
 *      Author: Conner Ebbinghaus
 */

#include <iostream>
#include <vector>
#include <future>
#include "TBAApi.hpp"
#include "doublefann.h"
#include "fann_cpp.h"

int main() {
	FANN::neural_net nnet;
	nnet.create_shortcut(2, 24, 2);
	nnet.set_training_algorithm(FANN::TRAIN_RPROP);

	//unsigned int x = 2;

	std::vector<std::pair<double*, double*> > data;

	auto eventKeys = TBAApi("/events/2018/keys").get();
	for(const auto& eventKey : eventKeys)
	{
		std::cout << "Downloading data for event" << eventKey << "..." << std::endl;
		auto matchKeys = TBAApi(std::string("/event/") + eventKey.get<std::string>() + std::string("/matches/keys")).get();
		std::vector<std::future<std::pair<double*, double*> > > dataT;
		for(const auto& matchKey : matchKeys)
		{
			dataT.push_back(std::async([&eventKey, &matchKey]() {
				const auto match = TBAApi("/match/" + matchKey.get<std::string>()).get();
				double* input = new double[24];
				double* output = new double[2];
				if(!match["winning_alliance"].get<std::string>().empty())
				{

					try {

						std::future<void> blueScores = std::async([&match, &input, &eventKey]() {
							unsigned int i = 0;
							for(const auto& team : match["alliances"]["blue"]["team_keys"])
							{
								auto teamInfo = TBAApi("/team/" + team.get<std::string>() + "/event/" + eventKey.get<std::string>() + "/status").get();
								auto teamRecord = teamInfo["qual"]["ranking"]["record"];
								input[i] = teamRecord["wins"];
								input[i+1] = teamRecord["ties"];
								input[i+2] = teamRecord["losses"];
								input[i] /= input[i]+input[i+1]+input[i+2];
								input[i+1] /= input[i]+input[i+1]+input[i+2];
								input[i+2] /= input[i]+input[i+1]+input[i+2];
								input[i+3] = teamInfo["qual"]["ranking"]["rank"].get<double>() / 100.0;
								i+=4;
							}
						});

						std::future<void> redScores = std::async([&match, &input, &eventKey]() {
							unsigned int i = 12;
							for(const auto& team : match["alliances"]["red"]["team_keys"])
							{
								auto teamInfo = TBAApi("/team/" + team.get<std::string>() + "/event/" + eventKey.get<std::string>() + "/status").get();
								auto teamRecord = teamInfo["qual"]["ranking"]["record"];
								input[i] = teamRecord["wins"];
								input[i+1] = teamRecord["ties"];
								input[i+2] = teamRecord["losses"];
								input[i+3] = teamInfo["qual"]["ranking"]["rank"].get<double>() / 100.0;
								input[i] /= input[i]+input[i+1]+input[i+2];
								input[i+1] /= input[i]+input[i+1]+input[i+2];
								input[i+2] /= input[i]+input[i+1]+input[i+2];
								i+=4;
							}
						});



						output[0] = match["alliances"]["blue"]["score"].get<double>()/600.0;
						output[1] = match["alliances"]["red"]["score"].get<double>()/600.0;
						blueScores.wait();
						redScores.wait();
						std::cout << "#";
						std::cout.flush();
						return std::make_pair(input, output);
					} catch (...) {
						std::cout << "X";
						std::cout.flush();

					}
				}
				for(unsigned int i = 0; i < 24; i++)
				{
					input[i] = 0;
				}
				output[0] = 0;
				output[1] = 0;
				return std::make_pair(input, output);
			}));

		}
		std::cout << std::endl;
		for(auto& thread : dataT)
		{
			data.push_back(thread.get());
		}
//		x--;
//		if(x == 0)
//		{return 0;}
	}

	FANN::training_data tData;
	double* inputs[data.size()];
	double* outputs[data.size()];
	unsigned int i=0;
	for(auto& d : data)
	{
		inputs[i] = d.first;
		outputs[i] = d.second;
		i++;
	}
	std::cout << std::endl << "Training... ";
	tData.set_train_data(data.size(), 24, (double**)inputs, 2, (double**)outputs);
	nnet.set_train_stop_function(FANN::STOPFUNC_MSE);
	nnet.init_weights(tData);
	nnet.cascadetrain_on_data(tData, 15, 1, 0.015);
	//std::cout << "Error: " << error << std::endl;
	for(unsigned int j = 0; j < data.size(); j++)
	{
		delete[] inputs[j];
		delete[] outputs[j];
	}

	nnet.save("nnet.out");

}
