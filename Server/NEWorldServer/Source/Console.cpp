#include "Console.h"

void Print(string message, int level) {
	switch (level) {
	case MESSAGE_INFO:
		cout << "[INFO] ";
		break;
	case MESSAGE_WARNING:
		cout << "[WARNING] ";
		break;
	case MESSAGE_ERROR:
		cout << "[ERROR] ";
		break;
	}
	cout << message << endl;
}
