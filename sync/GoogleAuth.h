/*
 * Ringbinder - a tabbed personal organizer for Haiku
 * Distributed under the terms of the MIT License.
 *
 * OAuth 2.0 for installed apps: authorization code flow with PKCE and a
 * loopback redirect (http://127.0.0.1:<port>), as recommended by Google for
 * desktop applications.
 */
#ifndef RB_GOOGLE_AUTH_H
#define RB_GOOGLE_AUTH_H

#include <string>

#include "Http.h"


namespace GoogleAuth {

extern const char* kScopes;

struct Tokens {
	std::string	accessToken;
	std::string	refreshToken;
	long		expiresIn;
	std::string	error;		// empty on success

	Tokens() : expiresIn(0) {}
};


class LoopbackFlow {
public:
							LoopbackFlow();
							~LoopbackFlow();

			// Opens the local listening socket on a random port.
			bool			Start(std::string& error);
			std::string		RedirectUri() const;
			std::string		AuthorizationUrl(const std::string& clientId,
								const std::string& scopes) const;

			// Waits for the browser to come back with the code. Polls
			// *cancel once a second; returns false on cancel, timeout,
			// denial or a state mismatch.
			bool			WaitForCode(int timeoutSeconds,
								volatile int* cancel, std::string& code,
								std::string& error);

			Tokens			Exchange(HttpTransport& http,
								const std::string& clientId,
								const std::string& clientSecret,
								const std::string& code) const;

private:
			int				fSocket;
			int				fPort;
			std::string		fVerifier;
			std::string		fState;
};


Tokens Refresh(HttpTransport& http, const std::string& clientId,
	const std::string& clientSecret, const std::string& refreshToken);

// Helpers, exposed for tests
std::string RandomToken(size_t length);
std::string Sha256(const std::string& data);	// 32 raw bytes
std::string Base64Url(const std::string& data);	// no padding
std::string QueryParam(const std::string& query, const std::string& name);

}	// namespace GoogleAuth

#endif // RB_GOOGLE_AUTH_H
