#pragma once
#include <string>

using std::string;

enum { MESSAGE_INFO, MESSAGE_WARNING, MESSAGE_ERROR };

void Print(string message, int level = MESSAGE_INFO);

string toString(int i);