/*
 * Ringbinder - a tabbed personal organizer for Haiku
 * Distributed under the terms of the MIT License.
 */
#include "GoogleAuth.h"

#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <time.h>
#include <unistd.h>

#include "Json.h"


namespace GoogleAuth {

const char* kScopes = "https://www.googleapis.com/auth/calendar.events "
	"https://www.googleapis.com/auth/contacts";

static const char* kAuthEndpoint
	= "https://accounts.google.com/o/oauth2/v2/auth";
static const char* kTokenEndpoint = "https://oauth2.googleapis.com/token";


// #pragma mark - SHA-256 (FIPS 180-4)


static inline uint32_t
Rotr(uint32_t x, int n)
{
	return (x >> n) | (x << (32 - n));
}


std::string
Sha256(const std::string& data)
{
	static const uint32_t k[64] = {
		0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5, 0x3956c25b,
		0x59f111f1, 0x923f82a4, 0xab1c5ed5, 0xd807aa98, 0x12835b01,
		0x243185be, 0x550c7dc3, 0x72be5d74, 0x80deb1fe, 0x9bdc06a7,
		0xc19bf174, 0xe49b69c1, 0xefbe4786, 0x0fc19dc6, 0x240ca1cc,
		0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da, 0x983e5152,
		0xa831c66d, 0xb00327c8, 0xbf597fc7, 0xc6e00bf3, 0xd5a79147,
		0x06ca6351, 0x14292967, 0x27b70a85, 0x2e1b2138, 0x4d2c6dfc,
		0x53380d13, 0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85,
		0xa2bfe8a1, 0xa81a664b, 0xc24b8b70, 0xc76c51a3, 0xd192e819,
		0xd6990624, 0xf40e3585, 0x106aa070, 0x19a4c116, 0x1e376c08,
		0x2748774c, 0x34b0bcb5, 0x391c0cb3, 0x4ed8aa4a, 0x5b9cca4f,
		0x682e6ff3, 0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208,
		0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2
	};
	uint32_t h[8] = { 0x6a09e667, 0xbb67ae85, 0x3c6ef372, 0xa54ff53a,
		0x510e527f, 0x9b05688c, 0x1f83d9ab, 0x5be0cd19 };

	std::string message = data;
	uint64_t bitLength = (uint64_t)data.size() * 8;
	message += (char)0x80;
	while (message.size() % 64 != 56)
		message += (char)0;
	for (int i = 7; i >= 0; i--)
		message += (char)((bitLength >> (i * 8)) & 0xff);

	for (size_t chunk = 0; chunk < message.size(); chunk += 64) {
		uint32_t w[64];
		for (int i = 0; i < 16; i++) {
			const unsigned char* p
				= (const unsigned char*)message.data() + chunk + i * 4;
			w[i] = ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16)
				| ((uint32_t)p[2] << 8) | p[3];
		}
		for (int i = 16; i < 64; i++) {
			uint32_t s0 = Rotr(w[i - 15], 7) ^ Rotr(w[i - 15], 18)
				^ (w[i - 15] >> 3);
			uint32_t s1 = Rotr(w[i - 2], 17) ^ Rotr(w[i - 2], 19)
				^ (w[i - 2] >> 10);
			w[i] = w[i - 16] + s0 + w[i - 7] + s1;
		}
		uint32_t a = h[0], b = h[1], c = h[2], d = h[3], e = h[4], f = h[5],
			g = h[6], hh = h[7];
		for (int i = 0; i < 64; i++) {
			uint32_t S1 = Rotr(e, 6) ^ Rotr(e, 11) ^ Rotr(e, 25);
			uint32_t ch = (e & f) ^ (~e & g);
			uint32_t t1 = hh + S1 + ch + k[i] + w[i];
			uint32_t S0 = Rotr(a, 2) ^ Rotr(a, 13) ^ Rotr(a, 22);
			uint32_t maj = (a & b) ^ (a & c) ^ (b & c);
			uint32_t t2 = S0 + maj;
			hh = g; g = f; f = e; e = d + t1;
			d = c; c = b; b = a; a = t1 + t2;
		}
		h[0] += a; h[1] += b; h[2] += c; h[3] += d;
		h[4] += e; h[5] += f; h[6] += g; h[7] += hh;
	}

	std::string digest;
	for (int i = 0; i < 8; i++) {
		for (int j = 3; j >= 0; j--)
			digest += (char)((h[i] >> (j * 8)) & 0xff);
	}
	return digest;
}


std::string
Base64Url(const std::string& data)
{
	static const char* kAlphabet
		= "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_";
	std::string out;
	size_t i = 0;
	const unsigned char* p = (const unsigned char*)data.data();
	for (; i + 2 < data.size(); i += 3) {
		uint32_t v = (p[i] << 16) | (p[i + 1] << 8) | p[i + 2];
		out += kAlphabet[(v >> 18) & 63];
		out += kAlphabet[(v >> 12) & 63];
		out += kAlphabet[(v >> 6) & 63];
		out += kAlphabet[v & 63];
	}
	if (i < data.size()) {
		uint32_t v = p[i] << 16;
		if (i + 1 < data.size())
			v |= p[i + 1] << 8;
		out += kAlphabet[(v >> 18) & 63];
		out += kAlphabet[(v >> 12) & 63];
		if (i + 1 < data.size())
			out += kAlphabet[(v >> 6) & 63];
	}
	return out;
}


std::string
RandomToken(size_t length)
{
	static const char* kUnreserved = "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
		"abcdefghijklmnopqrstuvwxyz0123456789-._~";
	std::string bytes;
	FILE* random = fopen("/dev/urandom", "rb");
	if (random != NULL) {
		char buffer[256];
		size_t got = fread(buffer, 1, length < sizeof(buffer)
			? length : sizeof(buffer), random);
		bytes.assign(buffer, got);
		fclose(random);
	}
	// Fallback mixing if /dev/urandom is short (should not happen)
	struct timeval now;
	gettimeofday(&now, NULL);
	uint32_t seed = (uint32_t)now.tv_usec ^ (uint32_t)now.tv_sec
		^ (uint32_t)getpid();
	std::string out;
	for (size_t i = 0; i < length; i++) {
		seed = seed * 1103515245u + 12345u;
		unsigned char c = i < bytes.size()
			? (unsigned char)bytes[i] : (unsigned char)(seed >> 16);
		out += kUnreserved[c % 66];
	}
	return out;
}


static std::string
UrlDecode(const std::string& text)
{
	std::string out;
	for (size_t i = 0; i < text.size(); i++) {
		char c = text[i];
		if (c == '+')
			out += ' ';
		else if (c == '%' && i + 2 < text.size()) {
			char hex[3] = { text[i + 1], text[i + 2], 0 };
			out += (char)strtol(hex, NULL, 16);
			i += 2;
		} else
			out += c;
	}
	return out;
}


std::string
QueryParam(const std::string& query, const std::string& name)
{
	size_t start = 0;
	while (start <= query.size()) {
		size_t end = query.find('&', start);
		if (end == std::string::npos)
			end = query.size();
		std::string pair = query.substr(start, end - start);
		size_t equals = pair.find('=');
		std::string key = UrlDecode(pair.substr(0, equals));
		if (key == name) {
			return equals == std::string::npos
				? std::string() : UrlDecode(pair.substr(equals + 1));
		}
		start = end + 1;
	}
	return std::string();
}


static Tokens
ParseTokenResponse(const HttpResponse& response)
{
	Tokens tokens;
	if (response.status == 0) {
		tokens.error = "Could not reach Google: " + response.error;
		return tokens;
	}
	Json json;
	if (!Json::Parse(response.body, json) || !json.IsObject()) {
		char buffer[64];
		snprintf(buffer, sizeof(buffer), "Unexpected reply from Google "
			"(HTTP %ld)", response.status);
		tokens.error = buffer;
		return tokens;
	}
	if (!response.Ok() || json.Has("error")) {
		std::string error = json.StrAt("error");
		std::string description = json.StrAt("error_description");
		if (error == "invalid_grant") {
			tokens.error = "Google no longer accepts Ringbinder's access "
				"(it was revoked or expired). Please connect again.";
		} else {
			tokens.error = "Google sign-in failed: " + error;
			if (!description.empty())
				tokens.error += " \xE2\x80\x94 " + description;
		}
		return tokens;
	}
	tokens.accessToken = json.StrAt("access_token");
	tokens.refreshToken = json.StrAt("refresh_token");
	tokens.expiresIn = (long)json.Get("expires_in").Num(3600);
	if (tokens.accessToken.empty())
		tokens.error = "Google did not return an access token.";
	return tokens;
}


Tokens
Refresh(HttpTransport& http, const std::string& clientId,
	const std::string& clientSecret, const std::string& refreshToken)
{
	FormFields form;
	form.push_back(std::make_pair("client_id", clientId));
	if (!clientSecret.empty())
		form.push_back(std::make_pair("client_secret", clientSecret));
	form.push_back(std::make_pair("grant_type", "refresh_token"));
	form.push_back(std::make_pair("refresh_token", refreshToken));

	std::vector<std::string> headers;
	headers.push_back("Content-Type: application/x-www-form-urlencoded");
	return ParseTokenResponse(http.Send("POST", kTokenEndpoint, headers,
		FormEncode(form)));
}


// #pragma mark - LoopbackFlow


LoopbackFlow::LoopbackFlow()
	:
	fSocket(-1),
	fPort(0)
{
	fVerifier = RandomToken(64);
	fState = RandomToken(24);
}


LoopbackFlow::~LoopbackFlow()
{
	if (fSocket >= 0)
		close(fSocket);
}


bool
LoopbackFlow::Start(std::string& error)
{
	fSocket = socket(AF_INET, SOCK_STREAM, 0);
	if (fSocket < 0) {
		error = std::string("Could not open a local socket: ")
			+ strerror(errno);
		return false;
	}

	struct sockaddr_in address;
	memset(&address, 0, sizeof(address));
	address.sin_family = AF_INET;
	address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
	address.sin_port = 0;	// let the system pick a free port
	if (bind(fSocket, (struct sockaddr*)&address, sizeof(address)) != 0
		|| listen(fSocket, 4) != 0) {
		error = std::string("Could not listen on 127.0.0.1: ")
			+ strerror(errno);
		return false;
	}

	socklen_t length = sizeof(address);
	getsockname(fSocket, (struct sockaddr*)&address, &length);
	fPort = ntohs(address.sin_port);
	return true;
}


std::string
LoopbackFlow::RedirectUri() const
{
	char buffer[48];
	snprintf(buffer, sizeof(buffer), "http://127.0.0.1:%d", fPort);
	return buffer;
}


std::string
LoopbackFlow::AuthorizationUrl(const std::string& clientId,
	const std::string& scopes) const
{
	FormFields query;
	query.push_back(std::make_pair("client_id", clientId));
	query.push_back(std::make_pair("redirect_uri", RedirectUri()));
	query.push_back(std::make_pair("response_type", "code"));
	query.push_back(std::make_pair("scope", scopes));
	query.push_back(std::make_pair("code_challenge",
		Base64Url(Sha256(fVerifier))));
	query.push_back(std::make_pair("code_challenge_method", "S256"));
	query.push_back(std::make_pair("state", fState));
	query.push_back(std::make_pair("access_type", "offline"));
	query.push_back(std::make_pair("prompt", "consent"));
	return std::string(kAuthEndpoint) + "?" + FormEncode(query);
}


static void
SendPage(int client, const char* status, const char* title,
	const char* message)
{
	char body[1024];
	snprintf(body, sizeof(body),
		"<!doctype html><html><head><meta charset=\"utf-8\">"
		"<title>%s</title></head>"
		"<body style=\"font-family:sans-serif;background:#fdfaec;"
		"color:#1e2040;text-align:center;padding-top:15%%\">"
		"<h1>%s</h1><p>%s</p></body></html>", title, title, message);
	char header[256];
	snprintf(header, sizeof(header),
		"HTTP/1.1 %s\r\nContent-Type: text/html; charset=utf-8\r\n"
		"Content-Length: %lu\r\nConnection: close\r\n\r\n",
		status, (unsigned long)strlen(body));
	send(client, header, strlen(header), 0);
	send(client, body, strlen(body), 0);
}


bool
LoopbackFlow::WaitForCode(int timeoutSeconds, volatile int* cancel,
	std::string& code, std::string& error)
{
	time_t deadline = time(NULL) + timeoutSeconds;

	while (true) {
		if (cancel != NULL && *cancel != 0) {
			error = "Sign-in was cancelled.";
			return false;
		}
		if (time(NULL) > deadline) {
			error = "Timed out waiting for Google sign-in in the browser.";
			return false;
		}

		fd_set readSet;
		FD_ZERO(&readSet);
		FD_SET(fSocket, &readSet);
		struct timeval wait = { 1, 0 };
		int ready = select(fSocket + 1, &readSet, NULL, NULL, &wait);
		if (ready <= 0)
			continue;

		int client = accept(fSocket, NULL, NULL);
		if (client < 0)
			continue;

		// Read the request head (bounded)
		std::string request;
		char buffer[2048];
		while (request.find("\r\n\r\n") == std::string::npos
			&& request.size() < 16384) {
			fd_set clientSet;
			FD_ZERO(&clientSet);
			FD_SET(client, &clientSet);
			struct timeval clientWait = { 5, 0 };
			if (select(client + 1, &clientSet, NULL, NULL, &clientWait) <= 0)
				break;
			ssize_t got = recv(client, buffer, sizeof(buffer), 0);
			if (got <= 0)
				break;
			request.append(buffer, got);
		}

		// "GET /?state=...&code=... HTTP/1.1"
		size_t space = request.find(' ');
		size_t space2 = request.find(' ', space + 1);
		std::string target = space != std::string::npos
			&& space2 != std::string::npos
			? request.substr(space + 1, space2 - space - 1) : "";
		size_t question = target.find('?');
		std::string query = question != std::string::npos
			? target.substr(question + 1) : "";

		if (query.empty()) {
			// e.g. /favicon.ico — not our redirect, keep waiting
			SendPage(client, "404 Not Found", "Not found", "");
			close(client);
			continue;
		}

		std::string state = QueryParam(query, "state");
		std::string denied = QueryParam(query, "error");
		code = QueryParam(query, "code");

		if (state != fState) {
			SendPage(client, "400 Bad Request", "Sign-in failed",
				"The reply did not match this sign-in attempt. Please try "
				"again from Ringbinder.");
			close(client);
			error = "The browser reply did not match this sign-in attempt.";
			return false;
		}
		if (!denied.empty() || code.empty()) {
			SendPage(client, "200 OK", "Sign-in cancelled",
				"Ringbinder was not given access. You can close this tab.");
			close(client);
			error = denied == "access_denied"
				? "Access was not granted in the browser."
				: "Google sign-in failed: " + denied;
			return false;
		}

		SendPage(client, "200 OK", "Ringbinder is connected",
			"You can close this tab and return to Ringbinder.");
		close(client);
		return true;
	}
}


Tokens
LoopbackFlow::Exchange(HttpTransport& http, const std::string& clientId,
	const std::string& clientSecret, const std::string& code) const
{
	FormFields form;
	form.push_back(std::make_pair("client_id", clientId));
	if (!clientSecret.empty())
		form.push_back(std::make_pair("client_secret", clientSecret));
	form.push_back(std::make_pair("code", code));
	form.push_back(std::make_pair("code_verifier", fVerifier));
	form.push_back(std::make_pair("grant_type", "authorization_code"));
	form.push_back(std::make_pair("redirect_uri", RedirectUri()));

	std::vector<std::string> headers;
	headers.push_back("Content-Type: application/x-www-form-urlencoded");
	return ParseTokenResponse(http.Send("POST", kTokenEndpoint, headers,
		FormEncode(form)));
}

}	// namespace GoogleAuth
