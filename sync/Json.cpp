/*
 * Ringbinder - a tabbed personal organizer for Haiku
 * Distributed under the terms of the MIT License.
 */
#include "Json.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>


static const Json kNull;


namespace {

class Parser {
public:
	Parser(const std::string& text) : fText(text), fPos(0), fDepth(0) {}

	bool Parse(Json& out, std::string& error)
	{
		_Skip();
		if (!_Value(out, error))
			return false;
		_Skip();
		if (fPos != fText.size()) {
			error = "trailing characters";
			return false;
		}
		return true;
	}

private:
	void _Skip()
	{
		while (fPos < fText.size() && (fText[fPos] == ' '
				|| fText[fPos] == '\t' || fText[fPos] == '\n'
				|| fText[fPos] == '\r'))
			fPos++;
	}

	bool _Fail(std::string& error, const char* what)
	{
		char buffer[96];
		snprintf(buffer, sizeof(buffer), "%s at offset %lu", what,
			(unsigned long)fPos);
		error = buffer;
		return false;
	}

	bool _Literal(const char* word)
	{
		size_t length = 0;
		while (word[length] != '\0')
			length++;
		if (fText.compare(fPos, length, word) != 0)
			return false;
		fPos += length;
		return true;
	}

	bool _Value(Json& out, std::string& error)
	{
		if (fPos >= fText.size())
			return _Fail(error, "unexpected end");
		if (++fDepth > 200)
			return _Fail(error, "nesting too deep");

		bool ok;
		char c = fText[fPos];
		if (c == '{')
			ok = _Object(out, error);
		else if (c == '[')
			ok = _Array(out, error);
		else if (c == '"') {
			std::string value;
			ok = _String(value, error);
			if (ok)
				out = Json(value);
		} else if (c == 't' && _Literal("true")) {
			out = Json(true);
			ok = true;
		} else if (c == 'f' && _Literal("false")) {
			out = Json(false);
			ok = true;
		} else if (c == 'n' && _Literal("null")) {
			out = Json();
			ok = true;
		} else if (c == '-' || (c >= '0' && c <= '9'))
			ok = _Number(out, error);
		else
			ok = _Fail(error, "unexpected character");

		fDepth--;
		return ok;
	}

	bool _Number(Json& out, std::string& error)
	{
		size_t start = fPos;
		if (fText[fPos] == '-')
			fPos++;
		while (fPos < fText.size()
			&& ((fText[fPos] >= '0' && fText[fPos] <= '9')
				|| fText[fPos] == '.' || fText[fPos] == 'e'
				|| fText[fPos] == 'E' || fText[fPos] == '+'
				|| fText[fPos] == '-'))
			fPos++;
		std::string number = fText.substr(start, fPos - start);
		char* end;
		double value = strtod(number.c_str(), &end);
		if (number.empty() || *end != '\0')
			return _Fail(error, "bad number");
		out = Json(value);
		return true;
	}

	static void _AppendUtf8(std::string& out, unsigned long code)
	{
		if (code < 0x80)
			out += (char)code;
		else if (code < 0x800) {
			out += (char)(0xC0 | (code >> 6));
			out += (char)(0x80 | (code & 0x3F));
		} else if (code < 0x10000) {
			out += (char)(0xE0 | (code >> 12));
			out += (char)(0x80 | ((code >> 6) & 0x3F));
			out += (char)(0x80 | (code & 0x3F));
		} else {
			out += (char)(0xF0 | (code >> 18));
			out += (char)(0x80 | ((code >> 12) & 0x3F));
			out += (char)(0x80 | ((code >> 6) & 0x3F));
			out += (char)(0x80 | (code & 0x3F));
		}
	}

	bool _Hex4(unsigned long& value)
	{
		if (fPos + 4 > fText.size())
			return false;
		value = 0;
		for (int i = 0; i < 4; i++) {
			char c = fText[fPos++];
			value <<= 4;
			if (c >= '0' && c <= '9')
				value |= c - '0';
			else if (c >= 'a' && c <= 'f')
				value |= c - 'a' + 10;
			else if (c >= 'A' && c <= 'F')
				value |= c - 'A' + 10;
			else
				return false;
		}
		return true;
	}

	bool _String(std::string& out, std::string& error)
	{
		fPos++;	// opening quote
		while (fPos < fText.size()) {
			char c = fText[fPos++];
			if (c == '"')
				return true;
			if ((unsigned char)c < 0x20)
				return _Fail(error, "control character in string");
			if (c != '\\') {
				out += c;
				continue;
			}
			if (fPos >= fText.size())
				break;
			char escape = fText[fPos++];
			switch (escape) {
				case '"': out += '"'; break;
				case '\\': out += '\\'; break;
				case '/': out += '/'; break;
				case 'b': out += '\b'; break;
				case 'f': out += '\f'; break;
				case 'n': out += '\n'; break;
				case 'r': out += '\r'; break;
				case 't': out += '\t'; break;
				case 'u':
				{
					unsigned long code;
					if (!_Hex4(code))
						return _Fail(error, "bad \\u escape");
					if (code >= 0xD800 && code <= 0xDBFF) {
						unsigned long low;
						if (fPos + 1 < fText.size() && fText[fPos] == '\\'
							&& fText[fPos + 1] == 'u') {
							fPos += 2;
							if (!_Hex4(low) || low < 0xDC00 || low > 0xDFFF)
								return _Fail(error, "bad surrogate pair");
							code = 0x10000 + ((code - 0xD800) << 10)
								+ (low - 0xDC00);
						} else
							code = 0xFFFD;
					} else if (code >= 0xDC00 && code <= 0xDFFF)
						code = 0xFFFD;
					_AppendUtf8(out, code);
					break;
				}
				default:
					return _Fail(error, "bad escape");
			}
		}
		return _Fail(error, "unterminated string");
	}

	bool _Array(Json& out, std::string& error)
	{
		out = Json::Array();
		fPos++;
		_Skip();
		if (fPos < fText.size() && fText[fPos] == ']') {
			fPos++;
			return true;
		}
		while (true) {
			Json item;
			_Skip();
			if (!_Value(item, error))
				return false;
			out.Push(item);
			_Skip();
			if (fPos >= fText.size())
				return _Fail(error, "unterminated array");
			if (fText[fPos] == ',') {
				fPos++;
				continue;
			}
			if (fText[fPos] == ']') {
				fPos++;
				return true;
			}
			return _Fail(error, "expected , or ]");
		}
	}

	bool _Object(Json& out, std::string& error)
	{
		out = Json::Object();
		fPos++;
		_Skip();
		if (fPos < fText.size() && fText[fPos] == '}') {
			fPos++;
			return true;
		}
		while (true) {
			_Skip();
			if (fPos >= fText.size() || fText[fPos] != '"')
				return _Fail(error, "expected member name");
			std::string key;
			if (!_String(key, error))
				return false;
			_Skip();
			if (fPos >= fText.size() || fText[fPos] != ':')
				return _Fail(error, "expected :");
			fPos++;
			_Skip();
			Json value;
			if (!_Value(value, error))
				return false;
			out.Set(key, value);
			_Skip();
			if (fPos >= fText.size())
				return _Fail(error, "unterminated object");
			if (fText[fPos] == ',') {
				fPos++;
				continue;
			}
			if (fText[fPos] == '}') {
				fPos++;
				return true;
			}
			return _Fail(error, "expected , or }");
		}
	}

	const std::string&	fText;
	size_t				fPos;
	int					fDepth;
};


void
DumpString(const std::string& value, std::string& out)
{
	out += '"';
	for (size_t i = 0; i < value.size(); i++) {
		unsigned char c = value[i];
		switch (c) {
			case '"': out += "\\\""; break;
			case '\\': out += "\\\\"; break;
			case '\n': out += "\\n"; break;
			case '\r': out += "\\r"; break;
			case '\t': out += "\\t"; break;
			case '\b': out += "\\b"; break;
			case '\f': out += "\\f"; break;
			default:
				if (c < 0x20) {
					char buffer[8];
					snprintf(buffer, sizeof(buffer), "\\u%04x", c);
					out += buffer;
				} else
					out += (char)c;
		}
	}
	out += '"';
}

}	// namespace


bool
Json::Parse(const std::string& text, Json& out, std::string* error)
{
	std::string message;
	Parser parser(text);
	bool ok = parser.Parse(out, message);
	if (!ok && error != NULL)
		*error = message;
	return ok;
}


std::string
Json::Dump() const
{
	std::string out;
	_Dump(out);
	return out;
}


void
Json::_Dump(std::string& out) const
{
	switch (fType) {
		case NUL:
			out += "null";
			break;
		case BOOL:
			out += fBool ? "true" : "false";
			break;
		case NUMBER:
		{
			char buffer[32];
			if (fNumber == floor(fNumber) && fabs(fNumber) < 1e15)
				snprintf(buffer, sizeof(buffer), "%.0f", fNumber);
			else
				snprintf(buffer, sizeof(buffer), "%.17g", fNumber);
			out += buffer;
			break;
		}
		case STRING:
			DumpString(fString, out);
			break;
		case ARRAY:
			out += '[';
			for (size_t i = 0; i < fItems.size(); i++) {
				if (i > 0)
					out += ',';
				fItems[i]._Dump(out);
			}
			out += ']';
			break;
		case OBJECT:
			out += '{';
			for (size_t i = 0; i < fMembers.size(); i++) {
				if (i > 0)
					out += ',';
				DumpString(fMembers[i].first, out);
				out += ':';
				fMembers[i].second._Dump(out);
			}
			out += '}';
			break;
	}
}


bool
Json::Has(const std::string& key) const
{
	for (size_t i = 0; i < fMembers.size(); i++) {
		if (fMembers[i].first == key)
			return true;
	}
	return false;
}


const Json&
Json::Get(const std::string& key) const
{
	for (size_t i = 0; i < fMembers.size(); i++) {
		if (fMembers[i].first == key)
			return fMembers[i].second;
	}
	return kNull;
}


Json&
Json::Set(const std::string& key, const Json& value)
{
	if (fType != OBJECT) {
		*this = Object();
	}
	for (size_t i = 0; i < fMembers.size(); i++) {
		if (fMembers[i].first == key) {
			fMembers[i].second = value;
			return fMembers[i].second;
		}
	}
	fMembers.push_back(std::make_pair(key, value));
	return fMembers.back().second;
}


Json&
Json::At(const std::string& key)
{
	if (fType != OBJECT)
		*this = Object();
	for (size_t i = 0; i < fMembers.size(); i++) {
		if (fMembers[i].first == key)
			return fMembers[i].second;
	}
	fMembers.push_back(std::make_pair(key, Json()));
	return fMembers.back().second;
}


void
Json::Remove(const std::string& key)
{
	for (size_t i = 0; i < fMembers.size(); i++) {
		if (fMembers[i].first == key) {
			fMembers.erase(fMembers.begin() + i);
			return;
		}
	}
}


size_t
Json::Size() const
{
	if (fType == ARRAY)
		return fItems.size();
	if (fType == OBJECT)
		return fMembers.size();
	return 0;
}


const Json&
Json::operator[](size_t index) const
{
	if (fType != ARRAY || index >= fItems.size())
		return kNull;
	return fItems[index];
}


void
Json::Push(const Json& value)
{
	if (fType != ARRAY)
		*this = Array();
	fItems.push_back(value);
}


void
Json::Erase(size_t index)
{
	if (fType == ARRAY && index < fItems.size())
		fItems.erase(fItems.begin() + index);
}


bool
Json::operator==(const Json& other) const
{
	if (fType != other.fType)
		return false;
	switch (fType) {
		case NUL: return true;
		case BOOL: return fBool == other.fBool;
		case NUMBER: return fNumber == other.fNumber;
		case STRING: return fString == other.fString;
		case ARRAY: return fItems == other.fItems;
		case OBJECT: return fMembers == other.fMembers;
	}
	return false;
}
