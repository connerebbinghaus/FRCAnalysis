/*
 * TBA.hpp
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
#include "json.hpp"
#include <sys/types.h>
#include <sys/stat.h>
#include <curlpp/cURLpp.hpp>
#include <curlpp/Easy.hpp>
#include <curlpp/Options.hpp>
#include <curlpp/Infos.hpp>
#ifndef WIN32
#include <unistd.h>
#endif

#ifdef WIN32
#define stat _stat
#endif

class TBAApi {
public:
	TBAApi(const std::string& url) : subUrl(url) {}
	nlohmann::json get() const
	{

		const std::string url(TBABaseUrl + subUrl);
		const std::string cachefilename(replaceChar(subUrl, '/', '_') + ".cache");
		//std::cout << cachefilename << std::endl;

		if(cache_control.count(subUrl) > 0 && cache_control.at(subUrl) < std::chrono::system_clock::now())
		{
			std::ifstream cachefile(cachefilename);
			nlohmann::json ret;
			cachefile >> ret;
			return ret;
		}

		curlpp::Easy request;
		request.setOpt<curlpp::options::Url>(url);

		std::list<std::string> headers;
		headers.push_back("X-TBA-Auth-Key: " + std::string(TBAAuthKey));

		struct stat buffer;
		if(stat(cachefilename.c_str(), &buffer) == 0)
		{

			headers.push_back("If-Modified-Since: " + formatTimeForRequest(buffer.st_mtime));

		}

		request.setOpt<curlpp::options::HttpHeader>(headers);

		std::stringstream buf;
		request.setOpt<curlpp::options::WriteStream>(&buf);
		request.setOpt<curlpp::options::SslVerifyPeer>(false); // SECURE!!!
		request.perform();

		if(curlpp::Infos::ResponseCode::get(request) == 200)
		{
			std::ofstream cachefile(cachefilename);
			cachefile << buf.rdbuf();
		}
		else if(curlpp::Infos::ResponseCode::get(request) == 304)
		{
			//std::cerr << "Using cache..." << std::endl;
		}
		else
		{
			std::cerr << "Request for " << url << " failed: Response was " << curlpp::Infos::ResponseCode::get(request) << std::endl;
		}

		std::ifstream cachefile(cachefilename);

		nlohmann::json ret;
		cachefile >> ret;
		return ret;
	}
private:
	static constexpr const char* TBABaseUrl = "https://www.thebluealliance.com/api/v3";
	static constexpr const char* TBAAuthKey = "WG5pUFbRtNL36CLKw071dPf3gdGeT16ngwuPTWhkQev1pvX2enVnf2hq2oPYtjCH";
	static std::string formatTimeForRequest(time_t time)
	{
		struct tm* timeinfo;
		timeinfo = gmtime(&time);
		char buffer[80];
		strftime (buffer,80,"%a, %d %b %Y %H:%M:%S GMT",timeinfo);
		return std::string(buffer);
	}

	static std::string replaceChar(std::string str, char ch1, char ch2) {
	  for (unsigned int i = 0; i < str.length(); ++i) {
	    if (str[i] == ch1)
	      str[i] = ch2;
	  }

	  return str;
	}

	const std::string subUrl;

	static std::map<std::string, std::chrono::system_clock::time_point> cache_control;
};


#endif /* TBAAPI_HPP_ */
