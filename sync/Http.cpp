/*
 * Ringbinder - a tabbed personal organizer for Haiku
 * Distributed under the terms of the MIT License.
 */
#include "Http.h"

#include <stdio.h>


std::string
UrlEncode(const std::string& text)
{
	static const char* kHex = "0123456789ABCDEF";
	std::string out;
	for (size_t i = 0; i < text.size(); i++) {
		unsigned char c = text[i];
		if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z')
			|| (c >= '0' && c <= '9') || c == '-' || c == '_' || c == '.'
			|| c == '~') {
			out += (char)c;
		} else {
			out += '%';
			out += kHex[c >> 4];
			out += kHex[c & 15];
		}
	}
	return out;
}


std::string
FormEncode(const FormFields& fields)
{
	std::string out;
	for (size_t i = 0; i < fields.size(); i++) {
		if (i > 0)
			out += '&';
		out += UrlEncode(fields[i].first);
		out += '=';
		out += UrlEncode(fields[i].second);
	}
	return out;
}


