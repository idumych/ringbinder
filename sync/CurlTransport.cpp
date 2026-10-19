/*
 * Ringbinder - a tabbed personal organizer for Haiku
 * Distributed under the terms of the MIT License.
 *
 * HttpTransport implemented with libcurl (curl_devel from HaikuDepot).
 */
#include "Http.h"

#include <curl/curl.h>


// #pragma mark - CurlTransport


static size_t
WriteCallback(char* data, size_t size, size_t count, void* user)
{
	std::string* body = static_cast<std::string*>(user);
	body->append(data, size * count);
	return size * count;
}


CurlTransport::CurlTransport()
{
	static bool sInitialized = false;
	if (!sInitialized) {
		curl_global_init(CURL_GLOBAL_DEFAULT);
		sInitialized = true;
	}
}


CurlTransport::~CurlTransport()
{
}


HttpResponse
CurlTransport::Send(const std::string& method, const std::string& url,
	const std::vector<std::string>& headers, const std::string& body)
{
	HttpResponse response;

	CURL* curl = curl_easy_init();
	if (curl == NULL) {
		response.error = "could not initialize libcurl";
		return response;
	}

	struct curl_slist* headerList = NULL;
	for (size_t i = 0; i < headers.size(); i++)
		headerList = curl_slist_append(headerList, headers[i].c_str());
	// Don't let libcurl add "Expect: 100-continue" to uploads
	headerList = curl_slist_append(headerList, "Expect:");

	char errorBuffer[CURL_ERROR_SIZE];
	errorBuffer[0] = '\0';

	curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
	curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, method.c_str());
	curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headerList);
	curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
	curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response.body);
	curl_easy_setopt(curl, CURLOPT_ERRORBUFFER, errorBuffer);
	curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
	curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 20L);
	curl_easy_setopt(curl, CURLOPT_TIMEOUT, 120L);
	curl_easy_setopt(curl, CURLOPT_NOSIGNAL, 1L);
	curl_easy_setopt(curl, CURLOPT_USERAGENT, "Ringbinder/1.1 (Haiku)");
	if (method != "GET" && method != "DELETE") {
		curl_easy_setopt(curl, CURLOPT_POSTFIELDS, body.c_str());
		curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, (long)body.size());
	}

	CURLcode result = curl_easy_perform(curl);
	if (result == CURLE_OK) {
		curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &response.status);
	} else {
		response.error = errorBuffer[0] != '\0'
			? errorBuffer : curl_easy_strerror(result);
	}

	curl_slist_free_all(headerList);
	curl_easy_cleanup(curl);
	return response;
}
