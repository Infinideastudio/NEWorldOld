cmake_minimum_required(VERSION 3.30)
project(glad)

add_library(glad STATIC glad.c)
target_include_directories(glad PUBLIC include/)
