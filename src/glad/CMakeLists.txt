cmake_minimum_required(VERSION 3.28)
project(glad)

add_library(glad STATIC glad.c)
target_include_directories(glad PUBLIC include/)
