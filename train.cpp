/*
 * train.cpp
 *
 *  Created on: Apr 14, 2018
 *      Author: Conner Ebbinghaus
 */

#include <iostream>
#include <iomanip>
#include "termcolor.hpp"
#include "doublefann.h"
#include "fann_cpp.h"

int main()
{
	FANN::neural_net nnet;

	nnet.create_shortcut(2, 24, 2);
	nnet.set_training_algorithm(FANN::TRAIN_RPROP);
	nnet.set_train_stop_function(FANN::STOPFUNC_MSE);
	FANN::training_data tData;
	std::cout << termcolor::cyan << "Loading data..." << termcolor::reset << std::endl;
	tData.read_train_from_file("data.dat");
	nnet.init_weights(tData);
	nnet.set_callback([](FANN::neural_net &net, FANN::training_data &train, unsigned int max_epochs, unsigned int epochs_between_reports, float desired_error, unsigned int epochs, void *user_data)
				{
					static unsigned int neurons = 0;
					neurons++;
					std::cout << "Neurons: " << termcolor::yellow << std::setw(3) << neurons << " (" << max_epochs << " max) " << termcolor::reset << "Error: " << termcolor::red << std::setw(8) << net.get_MSE() << " (" << desired_error << " desired.) " << termcolor::reset << "Epochs: " << termcolor::magenta << epochs << termcolor::reset << std::endl;
					return 0;
				}, nullptr);
	std::cout << termcolor::cyan << "Training..." << termcolor::reset << std::endl;
	nnet.cascadetrain_on_data(tData, 250, 1, 0.015);
	std::cout << termcolor::cyan << "Saving..." << termcolor::reset << std::endl;
	nnet.save("nnet.out");
	std::cout << termcolor::green << "Done!" << termcolor::reset << std::endl;
}

