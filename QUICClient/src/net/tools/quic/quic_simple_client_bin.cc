// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// A binary wrapper for QuicClient.
// Connects to a host using QUIC, sends a request to the provided URL, and
// displays the response.
//
// Some usage examples:
//
//   TODO(rtenneti): make --host optional by getting IP Address of URL's host.
//
//   Get IP address of the www.google.com
//   IP=`dig www.google.com +short | head -1`
//
// Standard request/response:
//   quic_client http://www.google.com  --host=${IP}
//   quic_client http://www.google.com --quiet  --host=${IP}
//   quic_client https://www.google.com --port=443  --host=${IP}
//
// Use a specific version:
//   quic_client http://www.google.com --quic_version=23  --host=${IP}
//
// Send a POST instead of a GET:
//   quic_client http://www.google.com --body="this is a POST body" --host=${IP}
//
// Append additional headers to the request:
//   quic_client http://www.google.com  --host=${IP}
//               --headers="Header-A: 1234; Header-B: 5678"
//
// Connect to a host different to the URL being requested:
//   Get IP address of the www.google.com
//   IP=`dig www.google.com +short | head -1`
//   quic_client mail.google.com --host=${IP}
//
// Try to connect to a host which does not speak QUIC:
//   Get IP address of the www.example.com
//   IP=`dig www.example.com +short | head -1`
//   quic_client http://www.example.com --host=${IP}

#include <iostream>
#include <cstdlib> // for exit()
#include <fstream>
#include <string>
#include <chrono>
#include <ctime>

#include <stdio.h>      /* printf */
#include <time.h>       /* clock_t, clock, CLOCKS_PER_SEC */
#include <math.h>       /* sqrt */

#include "base/at_exit.h"
#include "base/command_line.h"
#include "base/logging.h"
#include "base/message_loop/message_loop.h"
#include "net/base/net_errors.h"
#include "net/base/privacy_mode.h"
#include "net/cert/cert_verifier.h"
#include "net/cert/ct_known_logs.h"
#include "net/cert/ct_log_verifier.h"
#include "net/cert/multi_log_ct_verifier.h"
#include "net/http/transport_security_state.h"
#include "net/quic/chromium/crypto/proof_verifier_chromium.h"
#include "net/quic/core/quic_error_codes.h"
#include "net/quic/core/quic_packets.h"
#include "net/quic/core/quic_server_id.h"
#include "net/quic/platform/api/quic_socket_address.h"
#include "net/quic/platform/api/quic_str_cat.h"
#include "net/quic/platform/api/quic_string_piece.h"
#include "net/quic/platform/api/quic_text_utils.h"
#include "net/spdy/chromium/spdy_http_utils.h"
#include "net/spdy/core/spdy_header_block.h"
#include "net/tools/quic/quic_simple_client.h"
#include "net/tools/quic/synchronous_host_resolver.h"
#include "url/gurl.h"

using net::CertVerifier;
using net::CTPolicyEnforcer;
using net::CTVerifier;
using net::MultiLogCTVerifier;
using net::ProofVerifier;
using net::ProofVerifierChromium;
using net::QuicStringPiece;
using net::QuicTextUtils;
using net::SpdyHeaderBlock;
using net::TransportSecurityState;
using std::cout;
using std::cin;
using std::cerr;
using std::endl;
using std::string;
using namespace std;

// The IP or hostname the quic client will connect to.
string FLAGS_host = "";
// The tmp folder for downloading segment files.
string FLAGS_folder = "";
// The port to connect to.
int32_t FLAGS_port = 0;

int32_t FLAGS_max_segment = 0;
// If set, send a POST with this body.
string FLAGS_body = "";
// If set, contents are converted from hex to ascii, before sending as body of
// a POST. e.g. --body_hex=\"68656c6c6f\"
string FLAGS_body_hex = "";
// A semicolon separated list of key:value pairs to add to request headers.
string FLAGS_headers = "";
// Set to true for a quieter output experience.
bool FLAGS_quiet = false;
// QUIC version to speak, e.g. 21. If not set, then all available versions are
// offered in the handshake.
int32_t FLAGS_quic_version = -1;
// If true, a version mismatch in the handshake is not considered a failure.
// Useful for probing a server to determine if it speaks any version of QUIC.
bool FLAGS_version_mismatch_ok = false;
// If true, an HTTP response code of 3xx is considered to be a successful
// response, otherwise a failure.
bool FLAGS_redirect_is_success = true;
// Initial MTU of the connection.
int32_t FLAGS_initial_mtu = 0;
// run for benchmark
bool FLAGS_benchmark = false;
// bencmark file url
string FLAGS_benchmark_file_url = "";

int32_t FLAGS_max_repeat_count = 0;

class FakeProofVerifier: public ProofVerifier {
public:
	net::QuicAsyncStatus VerifyProof(const string& hostname, const uint16_t port, const string& server_config,
			net::QuicVersion quic_version, QuicStringPiece chlo_hash, const std::vector<string>& certs, const string& cert_sct,
			const string& signature, const net::ProofVerifyContext* context, string* error_details,
			std::unique_ptr<net::ProofVerifyDetails>* details, std::unique_ptr<net::ProofVerifierCallback> callback) override {
		return net::QUIC_SUCCESS;
	}

	net::QuicAsyncStatus VerifyCertChain(const std::string& hostname, const std::vector<std::string>& certs,
			const net::ProofVerifyContext* verify_context, std::string* error_details,
			std::unique_ptr<net::ProofVerifyDetails>* verify_details, std::unique_ptr<net::ProofVerifierCallback> callback) override {
		return net::QUIC_SUCCESS;
	}
};

int main(int argc, char* argv[]) {

	base::CommandLine::Init(argc, argv);
	base::CommandLine* line = base::CommandLine::ForCurrentProcess();
	const base::CommandLine::StringVector& mpd_url = line->GetArgs();

	logging::LoggingSettings settings;
	settings.logging_dest = logging::LOG_TO_SYSTEM_DEBUG_LOG;
	CHECK(logging::InitLogging(settings));
	string const EXIT_COMMAND = "exit";
	string segmentUrl;
	string fileName;

	long totalDownloadedBytes = 0;

	if (line->HasSwitch("h") || line->HasSwitch("help") || mpd_url.empty()) {
		const char* help_str = "Usage: quic_client [options] <mpd_url>\n"
				"\n"
				"<mpd_url> with scheme must be provided (e.g. http://www.google.com)\n\n"
				"Options:\n"
				"-h, --help                  show this help message and exit\n"
				"--host=<host>               specify the IP address of the hostname to "
				"connect to\n"
				"--port=<port>               specify the port to connect to\n"
				"--body=<body>               specify the body to post\n"
				"--body_hex=<body_hex>       specify the body_hex to be printed out\n"
				"--headers=<headers>         specify a semicolon separated list of "
				"key:value pairs to add to request headers\n"
				"--quiet                     specify for a quieter output experience\n"
				"--quic-version=<quic version> specify QUIC version to speak\n"
				"--version_mismatch_ok       if specified a version mismatch in the "
				"handshake is not considered a failure\n"
				"--redirect_is_success       if specified an HTTP response code of 3xx "
				"is considered to be a successful response, otherwise a failure\n"
				"--initial_mtu=<initial_mtu> specify the initial MTU of the connection"
				"\n"
				"--disable-certificate-verification do not verify certificates\n"
				"--folder=<segment_download_folder>\n";
		cout << help_str;
		exit(0);
	}
	if (line->HasSwitch("host")) {
		FLAGS_host = line->GetSwitchValueASCII("host");
	}
	if (line->HasSwitch("folder")) {
		FLAGS_folder = line->GetSwitchValueASCII("folder");
	}
	if (line->HasSwitch("port")) {
		if (!base::StringToInt(line->GetSwitchValueASCII("port"), &FLAGS_port)) {
			std::cerr << "--port must be an integer\n";
			return 1;
		}
	}
	if (line->HasSwitch("max_segment")) {
		if (!base::StringToInt(line->GetSwitchValueASCII("max_segment"), &FLAGS_max_segment)) {
			std::cerr << "--max_segment must be an integer\n";
			return 1;
		}
	}
	if (line->HasSwitch("max_repeat_count")) {
		if (!base::StringToInt(line->GetSwitchValueASCII("max_repeat_count"), &FLAGS_max_repeat_count)) {
			std::cerr << "--max_repeat_count must be an integer\n";
			return 1;
		}
	}
	if (line->HasSwitch("body")) {
		FLAGS_body = line->GetSwitchValueASCII("body");
	}
	if (line->HasSwitch("body_hex")) {
		FLAGS_body_hex = line->GetSwitchValueASCII("body_hex");
	}
	if (line->HasSwitch("headers")) {
		FLAGS_headers = line->GetSwitchValueASCII("headers");
	}
	if (line->HasSwitch("quiet")) {
		FLAGS_quiet = true;
	}
	if (line->HasSwitch("quic-version")) {
		int quic_version;
		if (base::StringToInt(line->GetSwitchValueASCII("quic-version"), &quic_version)) {
			FLAGS_quic_version = quic_version;
		}
	}
	if (line->HasSwitch("version_mismatch_ok")) {
		FLAGS_version_mismatch_ok = true;
	}
	if (line->HasSwitch("redirect_is_success")) {
		FLAGS_redirect_is_success = true;
	}
	if (line->HasSwitch("initial_mtu")) {
		if (!base::StringToInt(line->GetSwitchValueASCII("initial_mtu"), &FLAGS_initial_mtu)) {
			std::cerr << "--initial_mtu must be an integer\n";
			return 1;
		}
	}
	if (line->HasSwitch("benchmark")) {
		FLAGS_benchmark = true;
	}
	if (line->HasSwitch("benchmark_file_url")) {
		FLAGS_benchmark_file_url = line->GetSwitchValueASCII("benchmark_file_url");
	}

	VLOG(1) << "server host: " << FLAGS_host << " port: " << FLAGS_port << " body: " << FLAGS_body << " headers: " << FLAGS_headers
						<< " quiet: " << FLAGS_quiet << " quic-version: " << FLAGS_quic_version << " version_mismatch_ok: "
						<< FLAGS_version_mismatch_ok << " redirect_is_success: " << FLAGS_redirect_is_success << " initial_mtu: "
						<< FLAGS_initial_mtu << " folder:" << FLAGS_folder << " benchmark " << FLAGS_benchmark << " benchmark_file_url "
						<< FLAGS_benchmark_file_url;

	base::AtExitManager exit_manager;
	base::MessageLoopForIO message_loop;

	// Determine IP address to connect to from supplied hostname.
	net::QuicIpAddress ip_addr;

	GURL url(mpd_url[0]);
	string host = FLAGS_host;
	string folder = FLAGS_folder;

	if (host.empty()) {
		host = url.host();
	}
	int port = FLAGS_port;
	int max_segment = 150;
	if (line->HasSwitch("max_segment")) {
		max_segment = FLAGS_max_segment;
	}

	if (port == 0) {
		port = url.EffectiveIntPort();
	}
	if (!ip_addr.FromString(host)) {
		net::AddressList addresses;
		int rv = net::SynchronousHostResolver::Resolve(host, &addresses);
		if (rv != net::OK) {
			LOG(ERROR) << "Unable to resolve '" << host << "' : " << net::ErrorToShortString(rv);
			return 1;
		}
		ip_addr = net::QuicIpAddress(net::QuicIpAddressImpl(addresses[0].address()));
	}

	string host_port = net::QuicStrCat(ip_addr.ToString(), ":", port);
	VLOG(1) << "Resolved " << host << " to " << host_port << endl;

	// Build the client, and try to connect.
	net::QuicServerId server_id(url.host(), url.EffectiveIntPort(), net::PRIVACY_MODE_DISABLED);
	net::QuicVersionVector versions = net::AllSupportedVersions();
	if (FLAGS_quic_version != -1) {
		versions.clear();
		versions.push_back(static_cast<net::QuicVersion>(FLAGS_quic_version));
	}
	// For secure QUIC we need to verify the cert chain.
	std::unique_ptr<CertVerifier> cert_verifier(CertVerifier::CreateDefault());
	std::unique_ptr<TransportSecurityState> transport_security_state(new TransportSecurityState);
	std::unique_ptr<MultiLogCTVerifier> ct_verifier(new MultiLogCTVerifier());
	ct_verifier->AddLogs(net::ct::CreateLogVerifiersForKnownLogs());
	std::unique_ptr<CTPolicyEnforcer> ct_policy_enforcer(new CTPolicyEnforcer());
	std::unique_ptr<ProofVerifier> proof_verifier;
	if (line->HasSwitch("disable-certificate-verification")) {
		proof_verifier.reset(new FakeProofVerifier());
	} else {
		proof_verifier.reset(
				new ProofVerifierChromium(cert_verifier.get(), ct_policy_enforcer.get(), transport_security_state.get(),
						ct_verifier.get()));
	}
			net::QuicSimpleClient client(net::QuicSocketAddress(ip_addr, port), server_id, versions, std::move(proof_verifier));
			client.set_initial_max_packet_length(FLAGS_initial_mtu != 0 ? FLAGS_initial_mtu : net::kDefaultMaxPacketSize);
			if (!client.Initialize()) {
				cerr << "Failed to initialize client." << endl;
				return 1;
			}
			if (!client.Connect()) {
				net::QuicErrorCode error = client.session()->error();
				if (FLAGS_version_mismatch_ok && error == net::QUIC_INVALID_VERSION) {
					cout << "Server talks QUIC, but none of the versions supported by " << "this client: "
							<< QuicVersionVectorToString(versions) << endl;
					// Version mismatch is not deemed a failure.
					return 0;
				}
				cerr << "Failed to connect to " << host_port << ". Error: " << net::QuicErrorCodeToString(error) << endl;
				return 1;
			}
			cout << endl << "Connected to " << host_port << endl;

			// Construct the string body from flags, if provided.
			string body = FLAGS_body;
			if (!FLAGS_body_hex.empty()) {
				DCHECK(FLAGS_body.empty()) << "Only set one of --body and --body_hex.";
				body = QuicTextUtils::HexDecode(FLAGS_body_hex);
			}

			int segmentNo = 1;
			int maxRepeatCount = FLAGS_max_repeat_count;
			int runNo = 1;
			std::string strSegmentNo = "";
			const std::string originalBenchMarkUrl = FLAGS_benchmark_file_url;

			auto start = std::chrono::system_clock::now();

			std::time_t start_time = std::chrono::system_clock::to_time_t(start);
			std::cout << "started at " << std::ctime(&start_time) << endl;

			if (FLAGS_benchmark) {

				while (runNo <= maxRepeatCount) {
					cout << "current runNo: " << runNo << endl;
					while (segmentNo <= max_segment) {

						strSegmentNo = std::to_string(segmentNo);
						FLAGS_benchmark_file_url = originalBenchMarkUrl;
						cout << "segment No: " << segmentNo << endl;
						segmentUrl = FLAGS_benchmark_file_url.replace(FLAGS_benchmark_file_url.find("segment"), sizeof(strSegmentNo) - 1,
								strSegmentNo);

						GURL gurl(segmentUrl);

						// Construct a GET or POST request for supplied URL.
						SpdyHeaderBlock header_block;
						header_block[":method"] = body.empty() ? "GET" : "POST";
						header_block[":scheme"] = gurl.scheme();
						header_block[":authority"] = gurl.host();
						header_block[":path"] = gurl.path();

						// Append any additional headers supplied on the command line.
						for (QuicStringPiece sp : QuicTextUtils::Split(FLAGS_headers, ';')) {
							QuicTextUtils::RemoveLeadingAndTrailingWhitespace(&sp);
							if (sp.empty()) {
								continue;
							}
							std::vector<QuicStringPiece> kv = QuicTextUtils::Split(sp, ':');
							QuicTextUtils::RemoveLeadingAndTrailingWhitespace(&kv[0]);
							QuicTextUtils::RemoveLeadingAndTrailingWhitespace(&kv[1]);
							header_block[kv[0]] = kv[1];
						}

						// Make sure to store the response, for later output.
						client.set_store_response(true);

						// Send the request.
						client.SendRequestAndWaitForResponse(header_block, body, /*fin=*/true);

						// Print request and response details.
						if (!FLAGS_quiet) {
							if (!FLAGS_body_hex.empty()) {
								// Print the user provided hex, rather than binary body.
								cout << "body:\n" << QuicTextUtils::HexDump(QuicTextUtils::HexDecode(FLAGS_body_hex)) << endl;
							} else {
							}
							string response_body = client.latest_response_body();
							if (!FLAGS_body_hex.empty()) {
								// Assume response is binary data.
								cout << "body:\n" << QuicTextUtils::HexDump(response_body) << endl;
							} else {

								size_t found = segmentUrl.find_last_of("/");

								fileName = folder + segmentUrl.substr(found + 1);

								ofstream outf(fileName.c_str(), ios::binary);

								if (!outf) {
									// Print an error and exit
									cerr << fileName + " ERROR could not be opened for writing!" << endl;
									exit(1);
								}

								outf << response_body;
								cout << "file_size_start:" << response_body.size() << ":file_size_end ";

								totalDownloadedBytes = totalDownloadedBytes + response_body.size();
								cout << "total downloaded : " << totalDownloadedBytes << " ";
							}
						}

						size_t response_code = client.latest_response_code();
						if (response_code >= 200 && response_code < 300) {
							cout << "Request succeeded (" << response_code << ")." << endl;
						} else if (response_code >= 300 && response_code < 400) {
							if (FLAGS_redirect_is_success) {
								cout << "Request succeeded (redirect " << response_code << ")." << endl;
							} else {
								cout << "Request failed (redirect " << response_code << ")." << endl;
								return 1;
							}
						} else {
							cerr << "Request failed (" << response_code << ")." << endl;
							return 1;
						}
						segmentNo = segmentNo + 1;
					}
					segmentNo = 1;
					runNo = runNo + 1;

				}

				auto end = std::chrono::system_clock::now();
				start_time = std::chrono::system_clock::to_time_t(start);
				std::cout << "started at " << std::ctime(&start_time) << endl;

				std::chrono::duration<double> elapsed_seconds = end - start;
				std::time_t end_time = std::chrono::system_clock::to_time_t(end);
				std::cout << "finished computation at " << std::ctime(&end_time) << "elapsed time: " << elapsed_seconds.count() << endl;

			} else {

				while (true) {
					cin >> segmentUrl;
					if (segmentUrl.compare(EXIT_COMMAND) == 0) {
						cout << "Exiting..." << endl;

						auto end = std::chrono::system_clock::now();

						start_time = std::chrono::system_clock::to_time_t(start);
						std::cout << "started at " << std::ctime(&start_time) << endl;

						std::chrono::duration<double> elapsed_seconds = end - start;
						std::time_t end_time = std::chrono::system_clock::to_time_t(end);
						std::cout << "finished computation at " << std::ctime(&end_time) << "elapsed time: " << elapsed_seconds.count()
								<< endl;
						return 0;
					}

					GURL gurl(segmentUrl);

					// Construct a GET or POST request for supplied URL.
					SpdyHeaderBlock header_block;
					header_block[":method"] = body.empty() ? "GET" : "POST";
					header_block[":scheme"] = gurl.scheme();
					header_block[":authority"] = gurl.host();
					header_block[":path"] = gurl.path();

					// Append any additional headers supplied on the command line.
					for (QuicStringPiece sp : QuicTextUtils::Split(FLAGS_headers, ';')) {
						QuicTextUtils::RemoveLeadingAndTrailingWhitespace(&sp);
						if (sp.empty()) {
							continue;
						}
						std::vector<QuicStringPiece> kv = QuicTextUtils::Split(sp, ':');
						QuicTextUtils::RemoveLeadingAndTrailingWhitespace(&kv[0]);
						QuicTextUtils::RemoveLeadingAndTrailingWhitespace(&kv[1]);
						header_block[kv[0]] = kv[1];
					}

					// Make sure to store the response, for later output.
					client.set_store_response(true);

					// Send the request.
					client.SendRequestAndWaitForResponse(header_block, body, /*fin=*/true);

					// Print request and response details.
					if (!FLAGS_quiet) {
						if (!FLAGS_body_hex.empty()) {
							// Print the user provided hex, rather than binary body.
							cout << "body:\n" << QuicTextUtils::HexDump(QuicTextUtils::HexDecode(FLAGS_body_hex)) << endl;
						} else {
						}
						string response_body = client.latest_response_body();
						if (!FLAGS_body_hex.empty()) {
							// Assume response is binary data.
							cout << "body:\n" << QuicTextUtils::HexDump(response_body) << endl;
						} else {

							size_t found = segmentUrl.find_last_of("/");

							fileName = folder + segmentUrl.substr(found + 1);

							ofstream outf(fileName.c_str(), ios::binary);

							if (!outf) {
								// Print an error and exit
								cerr << fileName + " ERROR could not be opened for writing!" << endl;
								exit(1);
							}
							outf << response_body;
							cout << "file_size_start:" << response_body.size() << ":file_size_end ";

							totalDownloadedBytes = totalDownloadedBytes + response_body.size();
							cout << " total_downloaded : " << totalDownloadedBytes << " ";
						}
					}

					size_t response_code = client.latest_response_code();
					if (response_code >= 200 && response_code < 300) {
						cout << "Request succeeded (" << response_code << ")." << endl;
						//return 0;
					} else if (response_code >= 300 && response_code < 400) {
						if (FLAGS_redirect_is_success) {
							cout << "Request succeeded (redirect " << response_code << ")." << endl;
							//	return 0;
						} else {
							cout << "Request failed (redirect " << response_code << ")." << endl;
							return 1;
						}
					} else {
						cerr << "Request failed (" << response_code << ")." << endl;
						return 1;
					}
				} //while true read console
			} //else not benchmark
} //main

