//
// pch.h
//用于标准系统包含文件的头文件。
//
// 被生成系统用于生成预编译头。注意不需要
// pch.cpp，pch.h 自动包括在项目的
//所有 cpp 文件中
//

#include <jni.h>
#include <errno.h>

#include <string.h>
#include <unistd.h>
#include <sys/resource.h>

#include <EGL/egl.h>
#include <GLES/gl.h>

#include <android/sensor.h>

#include <android/log.h>

#include <memory>

#include "android_native_app_glue.h"
