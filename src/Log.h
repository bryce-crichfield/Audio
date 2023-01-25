#pragma once

#include <iostream>

#undef LOG_LEVEL_NONE
#define LOG_LEVEL_ERROR
#define LOG_LEVEL_WARN
#define LOG_LEVEL_INFO
#define LOG_LEVEL_DEBUG
#define LOG_LEVEL_SUCCESS

#define RED "\033[0;31m"
#define GREEN "\033[0;32m"
#define YELLOW "\033[0;33m"
#define BLUE "\033[0;34m"
#define MAGENTA "\033[0;35m"
#define RESET "\033[0m"

#ifdef LOG_LEVEL_ERROR
#ifndef LOG_LEVEL_NONE
#define LOG_ERROR(string) std::cout << RED << "[ERROR]\t" << string << RESET << std::endl;
#else
#define LOG_ERROR(...)
#endif
#else
#define LOG_ERROR(...)
#endif

#ifdef LOG_LEVEL_WARN
#ifndef LOG_LEVEL_NONE
#define LOG_WARN(string) std::cout << YELLOW << "[WARN]\t" << string << RESET << std::endl;
#else
#define LOG_WARN(...)
#endif
#else
#define LOG_WARN(...)
#endif

#ifdef LOG_LEVEL_INFO
#ifndef LOG_LEVEL_NONE
#define LOG_INFO(string) std::cout << BLUE << "[INFO]\t" << string << RESET << std::endl;
#else
#define LOG_INFO(...)
#endif
#else
#define LOG_INFO(...)
#endif

#ifdef LOG_LEVEL_DEBUG
#ifndef LOG_LEVEL_NONE
#define LOG_DEBUG(string) std::cout << MAGENTA << "[DEBUG]\t" << string << RESET << std::endl;
#else
#define LOG_DEBUG(...)
#endif
#else
#define LOG_DEBUG(...)
#endif

#ifdef LOG_LEVEL_SUCCESS
#ifndef LOG_LEVEL_NONE
#define LOG_SUCCESS(string) std::cout << GREEN << "[PASS]\t" << string << RESET << std::endl;
#else
#define LOG_SUCCESS(...)
#endif
#else
#define LOG_SUCCESS(...)
#endif
