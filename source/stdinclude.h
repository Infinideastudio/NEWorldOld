//#pragma warning(disable:4710) //����STL�߷����棺����δ����
//#pragma warning(disable:4514) //����STL�߷����棺δʹ�õ������������Ƴ�
//#pragma warning(disable:4350) //����STL�߷����棺��Ϊ����
//#pragma warning(push,0) //����ͷ�ļ��ľ���

#define NEWORLD_USE_WINAPI
#ifdef NEWORLD_USE_WINAPI
	#include <Windows.h>
	//#include <WinSock2.h>
#else
	#include <thread>
	#include <mutex>
#endif
#define _USE_MATH_DEFINES
#include <math.h>
#include <time.h>
#include <io.h>
#include <string>
#include <iostream>
#include <sstream>
#include <vector>
#include <memory>
#include <iomanip>
#include <fstream>
#include <map>

using std::string;
using std::vector;
using std::pair;
using std::unique_ptr;
using std::map;
using std::cout;
using std::endl;

//GLFW
#define GLFW_DLL
#define GLFW_INCLUDE_GLU
#include <GLFW/glfw3.h>
//GLEXT
#include <GL/glext.h>

//#pragma warning(pop)
//#pragma warning(disable:4820) //���Բ���Ҫ�ľ��棺���ݽṹ����
//#pragma warning(disable:4365) //���Բ���Ҫ�ľ��棺�з���/�޷��Ų�ƥ��