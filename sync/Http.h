/*
 * Ringbinder - a tabbed personal organizer for Haiku
 * Distributed under the terms of the MIT License.
 *
 * Minimal HTTP abstraction. The app uses CurlTransport; tests substitute a
 * fake that plays the part of Google's servers.
 */
#ifndef RB_HTTP_H
#define RB_HTTP_H

#include <string>
#include <utility>
#include <vector>


struct HttpResponse {
	long		status;		// 0 if the request never completed
	std::string	body;
	std::string	error;		// transport-level error text

	HttpResponse() : status(0) {}
	bool Ok() const { return status >= 200 && status < 300; }
};


class HttpTransport {
public:
	virtual ~HttpTransport() {}
	virtual HttpResponse Send(const std::string& method,
		const std::string& url, const std::vector<std::string>& headers,
		const std::string& body) = 0;
};


class CurlTransport : public HttpTransport {
public:
	CurlTransport();
	virtual ~CurlTransport();
	virtual HttpResponse Send(const std::string& method,
		const std::string& url, const std::vector<std::string>& headers,
		const std::string& body);
};


typedef std::vector<std::pair<std::string, std::string> > FormFields;

std::string UrlEncode(const std::string& text);
std::string FormEncode(const FormFields& fields);

#endif // RB_HTTP_H
