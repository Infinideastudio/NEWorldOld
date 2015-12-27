#include "Console.h"
#include <time.h>
#include <stdio.h>
void Print(string message, int level) {
	switch (level) {
	case MESSAGE_INFO:
		printf("[INFO]");
		break;
	case MESSAGE_WARNING:
		printf("[WARNING]");
		break;
	case MESSAGE_ERROR:
		printf("[ERROR]");
		break;
	}
	auto t = time(NULL);
	static tm lt;
	localtime_s(&lt, &t);
	printf("[%s%d:%s%d:%s%d] %s\n", lt.tm_hour < 10 ? "0" : "", lt.tm_hour, lt.tm_min < 10 ? "0" : "", lt.tm_min, lt.tm_sec < 10 ? "0" : "", lt.tm_sec, message.c_str());
}

string toString(int i) {
	char a[12];
	_itoa_s(i, a, 12, 10);
	return string(a);
}
