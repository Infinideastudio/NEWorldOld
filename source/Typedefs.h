#pragma once
#include <GL/GL.h>
#include <cstdint>

typedef uint8_t Brightness;
typedef uint16_t BlockID;
typedef uint16_t ItemID;
typedef uint64_t ChunkID;
typedef GLuint TextureID;
typedef GLuint VBOID;
typedef uint32_t SkinID;
typedef uint32_t OnlineID;
#ifdef NEWORLD_GAME
#ifdef NEWORLD_USE_WINAPI
typedef HANDLE Mutex_t;
typedef HANDLE Thread_t;
typedef PTHREAD_START_ROUTINE ThreadFunc_t;
#define ThreadFunc DWORD WINAPI
#else
typedef std::mutex* Mutex_t;
typedef std::thread* Thread_t;
typedef unsigned int(*ThreadFunc_t)(void* param);
#define ThreadFunc unsigned int
#endif
#endif
