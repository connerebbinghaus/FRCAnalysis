/*
 * TBAApi.hpp
 *
 *  Created on: Mar 18, 2018
 *      Author: Conner Ebbinghaus
 */

#ifndef TBAAPI_HPP_
#define TBAAPI_HPP_
#include <string>
#include <map>
#include <sstream>
#include <iostream>
#include <fstream>
#include <chrono>
#include <string_view>
#include "json.hpp"
#include <sys/types.h>
#include <sys/stat.h>
#include <curlpp/cURLpp.hpp>
#include <curlpp/Easy.hpp>
#include <curlpp/Options.hpp>
#include <curlpp/Infos.hpp>
#include "semaphore.hpp"
#ifndef WIN32
#include <unistd.h>
#endif

#ifdef WIN32
#define stat _stat
#endif
/**
 * This class takes a URL on the TBA Api. It then download the data at that address, parses the JSON, and returns the parsed result.
 * Obeys the Cache-Control header, meaning the data will be cached in files, to minimize redownloads.
 */
class TBAApi {
public:
	/**
	 * Constructor.
	 * @param url The path part of the api url.
	 */
	inline TBAApi(const std::string& url) : subUrl(url) {}

	/**
	 * Downloads and parses the data at the url.
	 * @return The parsed JSON object.
	 */
	inline nlohmann::json get() const
	{
		std::lock_guard urlGuard(urlMutex[subUrl]); // Only allow one download per url.
		const std::string url(TBABaseUrl + subUrl); // The full url to download
		const std::string cachefilename(replaceChar(subUrl, '/', '_') + ".cache"); // The cache filename to use.
		//std::cout << cachefilename << std::endl;

		curlpp::Easy request; // The libcurlpp request
		request.setOpt<curlpp::options::Url>(url); // Set the url we are downloading.

		std::list<std::string> headers; // The headers we will send
		headers.push_back("X-TBA-Auth-Key: " + std::string(TBAAuthKey)); // The TBA Auth key.

		struct stat buffer, buf2;
		if(stat(cachefilename.c_str(), &buffer) == 0 && stat(CacheControlFilename, &buf2) == 0) // Check for cache files and get modification time.
		{
			nlohmann::json index;
			bool parseGood = false;
			try
			{
				std::lock_guard guard(indexMutex);
				std::ifstream indexfile(CacheControlFilename);
				indexfile >> index; // Load the cache index file.
				parseGood = true;
			}
			catch(...)
			{
				parseGood = false;
			}
			if(parseGood && index.count(cachefilename) > 0 && std::chrono::system_clock::from_time_t(index[cachefilename]) > std::chrono::system_clock::now()) // Are we past the cachefile's expiration?
			{
				std::ifstream cachefile(cachefilename); // Yes, use the cache file and don't download.
				nlohmann::json ret;
				cachefile >> ret;
				//std::cout << "X";
				std::cout.flush();
				return ret;
			}

			// No, send the modification time so TBA can notify us if the data has not changed.
			headers.push_back("If-Modified-Since: " + formatTimeForRequest(buffer.st_mtime));
		}

		// Set the headers.
		request.setOpt<curlpp::options::HttpHeader>(headers);

		std::stringstream buf; // Stream to receive the data.
		std::map<std::string, std::string> recvHeaders; // Map to hold the recieved headers.

		request.setOpt<curlpp::options::WriteStream>(&buf); // Set our recieving stream.
		request.setOpt<curlpp::options::HeaderFunction>([&recvHeaders](char *buffer, size_t size, size_t nitems) { // Set a function to parse the headers and add them to our map.
			std::string_view data(buffer, size*nitems); // Get the data in a better format.
			auto colon = data.find_first_of(':'); // Find the colon.
			std::string_view name = data.substr(0, colon); // Get the Key.
			std::string_view value = data.substr(data.find_first_not_of(' ', colon+1), data.find_last_not_of(' ')); // Get the value.
			recvHeaders[std::string(name)] = std::string(value); // Add it to our map.
			return size*nitems; // Tell libcurl how much we processed.
		});

		request.setOpt<curlpp::options::SslVerifyPeer>(false); // SECURE!!!

		try
		{
			downloadSemaphore.wait();
			request.perform(); // Perform the request.
			downloadSemaphore.notify();
		}
		catch(...)
		{
			throw;
		}

		if(recvHeaders.count("Cache-Control")!=0)
		{
			//std::cout << recvHeaders["Cache-Control"] << std::endl;
		}

		if(curlpp::Infos::ResponseCode::get(request) == 200) // OK: We have the data.
		{
			std::ofstream cachefile(cachefilename);
			cachefile << buf.rdbuf(); // Save the data to the cache file.
			//std::cout << "*";
			//std::cout.flush();
		}
		else if(curlpp::Infos::ResponseCode::get(request) == 304) // NOT MODIFIED: Use the cache file. DON'T save the data we got.
		{
			//std::cout << "N";
			//std::cout.flush();
			//std::cerr << "Using cache..." << std::endl;
		}
		else
		{
			std::cerr << "Request for " << url << " failed: Response was " << curlpp::Infos::ResponseCode::get(request) << std::endl;
		}
		nlohmann::json index;
		try
		{
			std::lock_guard guard(indexMutex);
			std::ifstream indexfilein(CacheControlFilename);
			if(indexfilein.good())
			{
				indexfilein >> index; //  Read index file
			}
			indexfilein.close();
		}
		catch(...)
		{}
		index[cachefilename] = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now() + std::chrono::seconds(parseCacheControl(recvHeaders["Cache-Control"]))); // Save the cache expiration date to the index.

		try
		{
			std::lock_guard guard(indexMutex);
			std::ofstream indexfileout(CacheControlFilename, std::fstream::out|std::fstream::trunc);
			indexfileout << index; // Write index file.
			indexfileout.flush();
			indexfileout.close();
		}
		catch(...)
		{
			throw;
		}

		std::ifstream cachefile(cachefilename);

		try
		{
			nlohmann::json ret;
			cachefile >> ret; // Parse data from cache file.
			return ret;
		}
		catch(...)
		{
			throw;
		}
	}
private:
	static constexpr const char* TBABaseUrl = "https://www.thebluealliance.com/api/v3";
	static constexpr const char* TBAAuthKey = "WG5pUFbRtNL36CLKw071dPf3gdGeT16ngwuPTWhkQev1pvX2enVnf2hq2oPYtjCH";
	static constexpr const char* CacheControlFilename = "cache.dat";
	/**
	 * Formats a time for use in the If-Modified-Since header.
	 * @param time The time to format
	 * @return String representation suitable for If-Modified-Since header.
	 */
	inline static std::string formatTimeForRequest(time_t time)
	{
		struct tm* timeinfo;
		timeinfo = gmtime(&time);
		char buffer[80];
		strftime (buffer,80,"%a, %d %b %Y %H:%M:%S GMT",timeinfo);
		return std::string(buffer);
	}

	/**
	 * Replaces all occurances of a character in a string with another character.
	 * @param str The string to replace in.
	 * @param ch1 The character to replace.
	 * @param ch2 The character to replace with.
	 * @return The string with ch1 replace with ch2
	 */
	inline static std::string replaceChar(std::string str, char ch1, char ch2) {
	  for (unsigned int i = 0; i < str.length(); ++i) {
	    if (str[i] == ch1)
	      str[i] = ch2;
	  }

	  return str;
	}

	/**
	 * Parses the value of the Cache-Control header to get the max-age value.
	 * @param value The value of the Cache-Control header.
	 * @return The value for max-age (0 if not present).
	 */
	inline static int parseCacheControl(std::string value)
	{
		auto max_age = value.find_first_of("max-age");
		if(max_age != value.npos)
		{
			auto equals = value.find_first_of('=', max_age);
			auto begin = value.find_first_not_of(' ', equals+1);
			auto end = value.find_first_of(' ', begin);
			if(end == value.npos)
			{
				return std::stoi(value.substr(begin));
			}
			else
			{
				return std::stoi(value.substr(begin, end-begin));
			}
		}
		return 0;
	}

	const std::string subUrl;
	static std::mutex indexMutex;
	static Semaphore downloadSemaphore;
	static std::map<std::string, std::mutex> urlMutex;
};

std::mutex TBAApi::indexMutex;
std::map<std::string, std::mutex> TBAApi::urlMutex;
Semaphore TBAApi::downloadSemaphore(6); // Only allow 6 downloads at a time.
#endif /* TBAAPI_HPP_ */
