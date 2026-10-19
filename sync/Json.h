/*
 * Ringbinder - a tabbed personal organizer for Haiku
 * Distributed under the terms of the MIT License.
 *
 * A small JSON value type with a parser and writer, enough for the Google
 * REST APIs. Plain C++11, no Haiku dependencies, so it can be unit tested
 * anywhere. Object members keep their order.
 */
#ifndef RB_JSON_H
#define RB_JSON_H

#include <string>
#include <utility>
#include <vector>


class Json {
public:
	enum Type { NUL, BOOL, NUMBER, STRING, ARRAY, OBJECT };

	Json() : fType(NUL), fBool(false), fNumber(0) {}
	Json(bool value) : fType(BOOL), fBool(value), fNumber(0) {}
	Json(double value) : fType(NUMBER), fBool(false), fNumber(value) {}
	Json(int value) : fType(NUMBER), fBool(false), fNumber(value) {}
	Json(const char* value)
		: fType(STRING), fBool(false), fNumber(0), fString(value) {}
	Json(const std::string& value)
		: fType(STRING), fBool(false), fNumber(0), fString(value) {}

	static Json Array() { Json j; j.fType = ARRAY; return j; }
	static Json Object() { Json j; j.fType = OBJECT; return j; }

	static bool Parse(const std::string& text, Json& out,
		std::string* error = NULL);
	std::string Dump() const;

	Type type() const { return fType; }
	bool IsNull() const { return fType == NUL; }
	bool IsObject() const { return fType == OBJECT; }
	bool IsArray() const { return fType == ARRAY; }
	bool IsString() const { return fType == STRING; }

	// Lenient accessors: return a default when the type doesn't match
	std::string Str(const std::string& fallback = "") const
		{ return fType == STRING ? fString : fallback; }
	double Num(double fallback = 0) const
		{ return fType == NUMBER ? fNumber : fallback; }
	bool Bool(bool fallback = false) const
		{ return fType == BOOL ? fBool : fallback; }

	// Object access
	bool Has(const std::string& key) const;
	const Json& Get(const std::string& key) const;	// null Json if missing
	Json& Set(const std::string& key, const Json& value);
	Json& At(const std::string& key);	// creates a null member if missing
	void Remove(const std::string& key);
	const std::vector<std::pair<std::string, Json> >& Members() const
		{ return fMembers; }

	// Array access
	size_t Size() const;
	const Json& operator[](size_t index) const;
	Json& Item(size_t index) { return fItems[index]; }
	void Push(const Json& value);
	void Erase(size_t index);

	// Shortcut: Get(key).Str()
	std::string StrAt(const std::string& key) const
		{ return Get(key).Str(); }

	bool operator==(const Json& other) const;
	bool operator!=(const Json& other) const { return !(*this == other); }

private:
	void _Dump(std::string& out) const;

	Type	fType;
	bool	fBool;
	double	fNumber;
	std::string fString;
	std::vector<Json> fItems;
	std::vector<std::pair<std::string, Json> > fMembers;
};


#endif // RB_JSON_H
