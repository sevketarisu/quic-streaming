//============================================================================
// Name        : LibCurlCppConsole.cpp
// Author      : Sevket
// Version     :
// Copyright   : Your copyright notice
// Description : Hello World in C++, Ansi-style
//============================================================================

#include <iostream>
#include <string>
#include <cstdlib> // for exit()
#include <chrono>
#include <fstream>
#include <curl/curl.h>
#include <algorithm>
#include <stdlib.h>
#include <sstream>  // for string streams

#include <stdio.h>      /* printf */
#include <time.h>       /* clock_t, clock, CLOCKS_PER_SEC */
#include <math.h>       /* sqrt */

using std::cout;
using std::cin;
using std::cerr;
using std::endl;
using std::string;
using namespace std;

static size_t WriteCallback(void *contents, size_t size, size_t nmemb, void *userp) {
	((std::string*) userp)->append((char*) contents, size * nmemb);
	return size * nmemb;
}

char* getCmdOption(char ** begin, char ** end, const std::string & option) {
	char ** itr = std::find(begin, end, option);
	if (itr != end && ++itr != end) {
		return *itr;
	}
	return 0;
}

bool cmdOptionExists(char** begin, char** end, const std::string& option) {
	return std::find(begin, end, option) != end;
}

int main(int argc, char* argv[]) {
	CURL *curl;
	CURLcode res;

	string const EXIT_COMMAND = "exit";
	string segmentUrl;
	string fileName;
	string benchMarkFileUrl;
	bool FLAGS_benchmark = false;

	long totalDownloadedBytes = 0;

	char * folder = getCmdOption(argv, argv + argc, "-f");
	char * FLAGS_benchmark_file_url = getCmdOption(argv, argv + argc, "-benchmark_file_url");
	char * FLAGS_repeat_count = getCmdOption(argv, argv + argc, "-r");
	std::string originalBenchMarkUrl;
	std::string strRepeatCount;
	std::string strMaxSegment;

	if (cmdOptionExists(argv, argv + argc, "-benchmark")) {
		benchMarkFileUrl = FLAGS_benchmark_file_url;
		originalBenchMarkUrl = FLAGS_benchmark_file_url;
		FLAGS_benchmark = true;
	}

	int maxRepeatCount = 1;
	int runNo = 1;

	if (cmdOptionExists(argv, argv + argc, "-r")) {
		strRepeatCount = FLAGS_repeat_count;
		maxRepeatCount = atoi(strRepeatCount.c_str());
	}

	curl = curl_easy_init();

	if (curl) {

		/* enable TCP keep-alive for this transfer */
		curl_easy_setopt(curl, CURLOPT_TCP_KEEPALIVE, 1L);

		/* keep-alive idle time to 120 seconds */
		curl_easy_setopt(curl, CURLOPT_TCP_KEEPIDLE, 120L);

		/* interval time between keep-alive probes: 60 seconds */
		curl_easy_setopt(curl, CURLOPT_TCP_KEEPINTVL, 60L);

		curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
		//	curl_easy_setopt(curl, CURLOPT_NOSIGNAL, 1);
		curl_easy_setopt(curl, CURLOPT_TIMEOUT, 5L);

		cout << "Curl Initialized" << endl;

		int segmentNo = 1;
		std::string strSegmentNo = "";

		auto start = std::chrono::system_clock::now();
		std::time_t start_time = std::chrono::system_clock::to_time_t(start);

		std::cout << "started at " << std::ctime(&start_time);

		if (FLAGS_benchmark) {

			while (runNo <= maxRepeatCount) {
				cout << "current runNo: " << runNo << endl;
				while (segmentNo <= 150) {
					std::string readBuffer;
					stringstream ss;
					ss << segmentNo;
					strSegmentNo = ss.str();
					benchMarkFileUrl = originalBenchMarkUrl;

					cout << "segment No: " << segmentNo << endl;
					segmentUrl = benchMarkFileUrl.replace(benchMarkFileUrl.find("segment"), sizeof(strSegmentNo) - 1, strSegmentNo);
					cout << "segmentUrl: " << segmentUrl << endl;

					curl_easy_setopt(curl, CURLOPT_URL, segmentUrl.c_str());

					curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);

					curl_easy_setopt(curl, CURLOPT_WRITEDATA, &readBuffer);

					res = curl_easy_perform(curl);

					if (res != CURLE_OK) {
						cout << "Aborted";
						cout << endl;
						continue;
					}

					size_t found = segmentUrl.find_last_of("/");

					fileName = folder + segmentUrl.substr(found + 1);

					ofstream outf(fileName.c_str(), ios::binary);
					if (!outf) {
						// Print an error and exit
						cout << fileName + " ERROR could not be opened for writing!" << endl;
						exit(1);
					}

					outf << readBuffer;
					cout << "file_size_start:" << readBuffer.size() << ":file_size_end ";
					totalDownloadedBytes = totalDownloadedBytes + readBuffer.size();
					std::cout << " total_downloaded : " << totalDownloadedBytes << " ";
					if (res == 0) {
						cout << "Request succeeded (200).";
					} else {
						cout << "Request Failed";
					}

					readBuffer.clear();
					outf.close();

					segmentNo = segmentNo + 1;
				}
				segmentNo = 1;
				runNo = runNo + 1;
			}

			auto end = std::chrono::system_clock::now();
			std::chrono::duration<double> elapsed_seconds = end - start;
			std::time_t end_time = std::chrono::system_clock::to_time_t(end);
			start_time = std::chrono::system_clock::to_time_t(start);
			std::cout << "started at " << std::ctime(&start_time) << endl;
			std::cout << "finished at " << std::ctime(&end_time) << "elapsed time: " << elapsed_seconds.count() << endl;
			std::cout << "sum of total downloaded bytes: " << totalDownloadedBytes << endl;
		} else {

			while (true) {
				std::string readBuffer;
				cin >> segmentUrl;

				if (segmentUrl.compare(EXIT_COMMAND) == 0) {
					curl_easy_cleanup(curl);
					cout << "Exiting..." << endl;
					exit(0);
				}

				//CURL START
				curl_easy_setopt(curl, CURLOPT_URL, segmentUrl.c_str());
				curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
				curl_easy_setopt(curl, CURLOPT_WRITEDATA, &readBuffer);

				try {
					res = curl_easy_perform(curl);
				} catch (const std::exception& e) { // caught by reference to base
					cout << "CALL curl_easy_perform EXCEPTION: " << e.what() << endl;
					cout.flush();
					res = CURLE_AGAIN;
				}


				if (res != CURLE_OK) {
					readBuffer.clear();
					cout << "file_size_start:" << "-1" << ":file_size_end ";
					cout << "Request failed" << endl;
					cout.flush();
					continue;
				} else {

					size_t found = segmentUrl.find_last_of("/");
					fileName = folder + segmentUrl.substr(found + 1);

					ofstream outf(fileName.c_str(), ios::binary);
					if (!outf) {
						// Print an error and exit
						cout << fileName + " ERROR could not be opened for writing!" << endl;
						exit(1);
					}
					outf << readBuffer;
					cout << "file_size_start:" << readBuffer.size() << ":file_size_end ";
					if (res == 0) {
						cout << "Request succeeded (200)." << endl;
					}
					cout.flush();
					readBuffer.clear();
					outf.close();
				}
			}
		}
	}
	curl_easy_cleanup(curl);
}
