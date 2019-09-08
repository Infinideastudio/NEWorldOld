#pragma once
#include <string>
#include <iostream>

using std::string;
using std::cout;
using std::endl;

enum { MESSAGE_INFO, MESSAGE_WARNING, MESSAGE_ERROR };

void Print(std::stringmessage, int level = MESSAGE_INFO);

std::stringtoString(int i);