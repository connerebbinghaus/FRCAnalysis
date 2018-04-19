/*
 * main.cpp
 *
 *  Created on: Mar 18, 2018
 *      Author: Conner Ebbinghaus
 */

#include <iostream>
#include <vector>
#include <future>
#include <atomic>
#include "TBAApi.hpp"
#include "doublefann.h"
#include "fann_cpp.h"
#include "termcolor.hpp"
#include <iomanip>
/**
 * Downloads match data from TBA and formats it for use in FANN.
 * It saves the result in a file named "data.dat"
 * @return 0 on success.
 */
int main() {
	std::vector<std::pair<double*, double*> > data; // Storage for the data.

	auto eventKeys = TBAApi("/events/2018/keys").get(); // Download the list of events.
	for(const auto& eventKey : eventKeys) // For every event...
	{
		std::cout << "Downloading data for event " << termcolor::yellow << eventKey.get<std::string>() << "..." <<  termcolor::reset << std::endl;
		auto matchKeys = TBAApi(std::string("/event/") + eventKey.get<std::string>() + std::string("/matches/keys")).get(); // Get all matches in the event.
		std::vector<std::future<std::pair<double*, double*> > > dataT; // Storage for download job futures.
		std::cout << termcolor::cyan << std::setw(3) << matchKeys.size() << std::setw(0) << " matches..." << termcolor::reset << std::endl;
		std::atomic<int> matchesLeft(matchKeys.size());
		for(const auto& matchKey : matchKeys) // For every match...
		{
			dataT.push_back(std::async([&eventKey, &matchKey, &matchesLeft]() { // Queue and start a new download job.
				static std::mutex printMutex;
				const auto match = TBAApi("/match/" + matchKey.get<std::string>()).get(); // Download the match data.
				double* input = new double[24];
				double* output = new double[2];
				unsigned int i = 0;
				while(true)
				{
					if(!match["winning_alliance"].get<std::string>().empty()) // Make sure the match has been played.
					{
						try {
							std::future<void> blueScores = std::async([&match, &input, &eventKey]() { // Start a (deferred) job to get blue scores. Deferred because TBA does not like that many downloads at once.
								unsigned int i = 0;
								for(const auto& team : match["alliances"]["blue"]["team_keys"]) // For every team on the blue alliance...
								{
									auto teamInfo = TBAApi("/team/" + team.get<std::string>() + "/event/" + eventKey.get<std::string>() + "/status").get(); // Download the team's info.
									auto teamRecord = teamInfo["qual"]["ranking"]["record"]; // Get team record data.
									input[i] = teamRecord["wins"];
									input[i+1] = teamRecord["ties"];
									input[i+2] = teamRecord["losses"];
									input[i] /= input[i]+input[i+1]+input[i+2]; // Normalize data between 0 and 1.
									input[i+1] /= input[i]+input[i+1]+input[i+2];
									input[i+2] /= input[i]+input[i+1]+input[i+2];
									input[i+3] = teamInfo["qual"]["ranking"]["rank"].get<double>() / 100.0;
									i+=4;
								}
							});

							std::future<void> redScores = std::async([&match, &input, &eventKey]() { // Same sa above, but for the red alliance.
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
							blueScores.wait(); // Wait for the job to finish (because it is deferred, it actually runs now.)
							redScores.wait();
							printMutex.lock();
							matchesLeft--;
							std::cout << termcolor::cyan << std::setw(3) << matchesLeft.load() << " " << std::setw(5) << termcolor::magenta << match["comp_level"].get<std::string>() + std::to_string(match["match_number"].get<int>()) << std::setw(0) << ":\t" << termcolor::red << match["alliances"]["red"]["score"] << "\t" << termcolor::blue << match["alliances"]["blue"]["score"] << termcolor::reset << std::endl;
							std::cout.flush();
							printMutex.unlock();
							return std::make_pair(input, output);
						} catch (...) {
							if(i >= 10)
							{
								printMutex.lock();
								std::cout << termcolor::cyan << std::setw(3) << matchesLeft.load() << " " << std::setw(5) << termcolor::magenta << match["comp_level"].get<std::string>() + std::to_string(match["match_number"].get<int>()) << std::setw(0) << ": " << termcolor::red << "failed." << termcolor::reset << std::endl;
								std::cout.flush();
								printMutex.unlock();
								throw;
							}
							i++;
							printMutex.lock();
							std::cout << termcolor::cyan << std::setw(3) << matchesLeft.load() << " " << std::setw(5) << termcolor::magenta << match["comp_level"].get<std::string>() + std::to_string(match["match_number"].get<int>()) << std::setw(0) << ": " << termcolor::red << "error " << termcolor::yellow << i << termcolor::reset << std::endl;
							std::cout.flush();
							printMutex.unlock();
						}
					}
					else
					{
						printMutex.lock();
						matchesLeft--;
						std::cout << termcolor::cyan << std::setw(3) << matchesLeft.load() << " " << std::setw(5) << termcolor::magenta << match["comp_level"].get<std::string>() + std::to_string(match["match_number"].get<int>()) << std::setw(0) << ": " << termcolor::yellow << "no data." << termcolor::reset << std::endl;
						std::cout.flush();
						printMutex.unlock();
						throw std::runtime_error("Match has no data."); // Match has not been played, return zeros.
					}
				}
			}));

		}
		for(auto& thread : dataT)
		{
			try
			{
				data.push_back(thread.get()); // Wait for the jobs to complete, and transfer the data to our storage.
			}
			catch(...)
			{}
		}
		std::cout << std::endl;
//		x--;
//		if(x == 0)
//		{return 0;}
	}
	std::cout << termcolor::cyan << "Saving..." << termcolor::reset << std::endl;
	FANN::training_data tData;
	double* inputs[data.size()]; // Array for FANN.
	double* outputs[data.size()];
	unsigned int i=0;
	for(auto& d : data) // Transfer data to array.
	{
		inputs[i] = d.first;
		outputs[i] = d.second;
		i++;
	}
	//std::cout << std::endl << "Training... ";
	tData.set_train_data(data.size(), 24, (double**)inputs, 2, (double**)outputs); // Give FANN the data.
	tData.save_train("data.dat"); // Save the data/
	std::cout << termcolor::green << "Done!" << termcolor::reset << std::endl;
	for(unsigned int j = 0; j < data.size(); j++) // Clean up after ourselves.
	{
		delete[] inputs[j];
		delete[] outputs[j];
	}
	return 0;
}
