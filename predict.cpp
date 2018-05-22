/*
 * predict.cpp
 *
 *  Created on: Apr 27, 2018
 *      Author: Conner Ebbinghaus
 */
#include <iostream>
#include <iomanip>
#include <future>
#include "TBAApi.hpp"
#include "termcolor.hpp"
#include "doublefann.h"
#include "fann_cpp.h"

void term_handler()
{
	std::cerr << "Something went terribly wrong." << std::endl;
	std::abort();
}

struct Team {
	std::string key;
	int wins;
	int losses;
	int ties;
	int rankingPoints;
	int rank;

	int parkAndClimb;
	int autoPts;
	int ownership;
	int vault;
};

class Event
{
public:
	void init(const std::string& eventKey)
	{
		std::vector<nlohmann::json> qualmatches;
		auto matches = TBAApi("/event/" + eventKey + "/matches").get();
		for(const auto& match : matches)
		{
			if(match["comp_level"] == "qm")
			{
				qualmatches.push_back(match);
			}
		}

		std::sort(qualmatches.begin(), qualmatches.end(), [](const nlohmann::json& a, const nlohmann::json& b) {
			return a["match_number"] < b["match_number"];
		});

		std::map<std::string, Team> teamStates;

		for(const auto& match : qualmatches)
		{
			if(!match["alliances"]["red"]["score"].is_number() || match["alliances"]["red"]["score"] == -1)
			{
				data.at(match["key"].get<std::string>()) = std::map(teamStates);
				continue;
			}
			else if(match.count("score_breakdown") == 0 || !match["score_breakdown"].is_object())
			{
				throw std::runtime_error("Match " + match["key"].get<std::string>() + " has scores (meaning it has been played) but has no score breakdown. Rank calculations cannot continue.");
			}
			for(const auto& team : match["alliances"]["red"]["team_keys"])
			{
				Team t;
				if(teamStates.count(team.get<std::string>()) != 0)
				{
					t = teamStates.at(team.get<std::string>());
				}
				else
				{
					t.key = team.get<std::string>();
					t.losses = 0;
					t.rank = 0;
					t.rankingPoints = 0;
					t.ties = 0;
					t.wins = 0;
					t.parkAndClimb = 0;
					t.autoPts = 0;
					t.ownership = 0;
					t.vault = 0;
				}

				if(match["winning_alliance"] == "red")
				{
					t.wins++;
				}
				else if(match["winning_alliance"] == "blue")
				{
					t.losses++;
				}
				else
				{
					t.ties++;
				}

				t.rankingPoints += match["score_breakdown"]["red"]["rp"].get<int>();
				t.parkAndClimb += match["score_breakdown"]["red"]["endgamePoints"].get<int>();
				t.autoPts += match["score_breakdown"]["red"]["autoPoints"].get<int>();
				t.ownership += match["score_breakdown"]["red"]["teleopOwnershipPoints"].get<int>();
				t.vault += match["score_breakdown"]["red"]["vaultPoints"].get<int>();
			}

			for(const auto& team : match["alliances"]["blue"]["team_keys"])
			{
				Team t;
				if(teamStates.count(team.get<std::string>()) != 0)
				{
					t = teamStates.at(team.get<std::string>());
				}
				else
				{
					t.key = team.get<std::string>();
					t.losses = 0;
					t.rank = 0;
					t.rankingPoints = 0;
					t.ties = 0;
					t.wins = 0;
					t.parkAndClimb = 0;
					t.autoPts = 0;
					t.ownership = 0;
					t.vault = 0;
				}

				if(match["winning_alliance"] == "blue")
				{
					t.wins++;
				}
				else if(match["winning_alliance"] == "red")
				{
					t.losses++;
				}
				else
				{
					t.ties++;
				}

				t.rankingPoints += match["score_breakdown"]["blue"]["rp"].get<int>();
				t.parkAndClimb += match["score_breakdown"]["blue"]["endgamePoints"].get<int>();
				t.autoPts += match["score_breakdown"]["blue"]["autoPoints"].get<int>();
				t.ownership += match["score_breakdown"]["blue"]["teleopOwnershipPoints"].get<int>();
				t.vault += match["score_breakdown"]["blue"]["vaultPoints"].get<int>();
				teamStates[team.get<std::string>()] = t;
			}

			std::vector<Team> Teams;
			for(auto& team : teamStates)
			{
				Teams.push_back(team.second);
			}

			std::sort(Teams.begin(), Teams.end(), [](const Team& a, const Team& b) {
				if(a.rankingPoints != b.rankingPoints)
				{
					return a.rankingPoints < b.rankingPoints;
				}
				if(a.parkAndClimb != b.parkAndClimb)
				{
					return a.parkAndClimb < b.parkAndClimb;
				}
				if(a.autoPts != b.autoPts)
				{
					return a.autoPts < b.autoPts;
				}
				if(a.ownership != b.ownership)
				{
					return a.ownership < b.ownership;
				}
				if(a.vault != b.vault)
				{
					return a.vault < b.vault;
				}
				return false;
			});

			int rankCount = 1;

			for(Team& t : Teams)
			{
				t.rank = rankCount;
				rankCount++;
				teamStates.at(t.key) = t;
			}

			data[match["key"].get<std::string>()] = std::map(teamStates);

		}
		return;
	}

	Team getTeamStatusAtMatch(const std::string& team, const std::string& matchKey)
	{
		auto& match = data.at(matchKey);
		if(match.count(team) == 0)
		{
			throw std::runtime_error("No data available for team " + team + " for match " + matchKey + ". May be first match for team?");
		}
		return match.at(team);
	}

private:
	std::map<std::string, std::map<std::string, Team>> data;
};

int main()
{
	std::set_terminate(term_handler);
	FANN::neural_net nnet("nnet.out");
	std::cout << termcolor::cyan << "Getting list of events..." << termcolor::reset << std::endl;
	const auto events = TBAApi("/events/2018/simple").get();
	for(const auto& event : events)
	{
		std::cout << termcolor::yellow << event["key"].get<std::string>() << ": "
				<< termcolor::cyan << "\"" << event["name"].get<std::string>() << "\" "
				<< termcolor::green << event["city"].get<std::string>() << ", "
				<< event["state_prov"].get<std::string>() << ", "
				<< event["country"].get<std::string>() << " "
				<< termcolor::magenta << event["start_date"].get<std::string>() << " - " << event["end_date"].get<std::string>()
				<< termcolor::reset << std::endl;
	}
	std::cout << termcolor::cyan << "Enter event ID: " << termcolor::yellow;
	std::string eventKey;
	std::getline(std::cin, eventKey);

	Event eventData;

	std::future<void> initResult = std::async(&Event::init, &eventData, eventKey);

	std::cout << termcolor::cyan << "Getting list of matches..." << termcolor::reset << std::endl;

	try {
		auto matches = TBAApi("/event/" + eventKey + "/matches/simple").get();
		for(const auto& match : matches)
		{
			std::cout << termcolor::yellow << match["key"].get<std::string>() << std::setw(0) << ":  \t";
			if(!match["winning_alliance"].get<std::string>().empty())
			{
				std::cout << termcolor::red << match["alliances"]["red"]["score"] << "\t" << termcolor::blue << match["alliances"]["blue"]["score"] << termcolor::reset << std::endl;
			}
			else
			{
				std::cout << termcolor::magenta << "No Data." << termcolor::reset << std::endl;
			}
		}
	}
	catch(const std::exception& e)
	{
		std::cout << termcolor::red << "Exception while getting event matches: " << e.what() << termcolor::reset << std::endl;
		return 1;
	}

	std::cout << termcolor::cyan << "Enter match ID: " << termcolor::yellow;
	std::string matchKey;
	std::getline(std::cin, matchKey);

	auto match = TBAApi("/match/" + matchKey).get();

	std::cout << termcolor::cyan << "Waiting for initialization job to complete..." << termcolor::reset << std::endl;
	try {
		initResult.get();
	}
	catch(const std::exception& e)
	{
		std::cout << termcolor::red << "Initialization job failed with exception: " << e.what() << termcolor::reset << std::endl;
		return 2;
	}

	double* input = new double[24];
	unsigned int i = 0;
	try{
		for(const auto& team : match["alliances"]["blue"]["team_keys"]) // For every team on the blue alliance...
		{
			input[i] = eventData.getTeamStatusAtMatch(team.get<std::string>(), matchKey).wins;
			input[i+1] = eventData.getTeamStatusAtMatch(team.get<std::string>(), matchKey).ties;
			input[i+2] = eventData.getTeamStatusAtMatch(team.get<std::string>(), matchKey).losses;
			input[i] /= input[i]+input[i+1]+input[i+2]; // Normalize data between 0 and 1.
			input[i+1] /= input[i]+input[i+1]+input[i+2];
			input[i+2] /= input[i]+input[i+1]+input[i+2];
			input[i+3] = eventData.getTeamStatusAtMatch(team.get<std::string>(), matchKey).rank / 100.0;
			i+=4;
		}

		for(const auto& team : match["alliances"]["red"]["team_keys"]) // For every team on the blue alliance...
		{
			input[i] = eventData.getTeamStatusAtMatch(team.get<std::string>(), matchKey).wins;
			input[i+1] = eventData.getTeamStatusAtMatch(team.get<std::string>(), matchKey).ties;
			input[i+2] = eventData.getTeamStatusAtMatch(team.get<std::string>(), matchKey).losses;
			input[i] /= input[i]+input[i+1]+input[i+2]; // Normalize data between 0 and 1.
			input[i+1] /= input[i]+input[i+1]+input[i+2];
			input[i+2] /= input[i]+input[i+1]+input[i+2];
			input[i+3] = eventData.getTeamStatusAtMatch(team.get<std::string>(), matchKey).rank / 100.0;
			i+=4;
		}
	}
	catch(const std::exception& e)
	{
		std::cout << termcolor::red << "Failed getting team info: " << e.what() << termcolor::reset << std::endl;
		return 3;
	}
	double* output = nnet.run(input);
	std::cout << termcolor::cyan << "Prediction:" << termcolor::reset << std::endl;
	if(output[0] < 0)
	{
		std::cout << termcolor::red << "Red wins by " << -output[0]*600 << " points." << termcolor::reset << std::endl;
	}
	else
	{
		std::cout << termcolor::red << "Blue wins by " << output[0]*600 << " points." << termcolor::reset << std::endl;
	}
	return 0;
}

