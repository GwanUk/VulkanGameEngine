#pragma once

#include <iostream>
#include <cassert>
#include <string>
#include <format>
#include <vulkan/vulkan.h>

#define VK_CHECK(result) vkCheck(result, __FILE__, __LINE__)

namespace guk {

void vkCheck(VkResult result, const char* file, int line);

template <typename... Args>
static void log(std::format_string<Args...> fmt, Args&&... args)
{
    std::string message = std::format(fmt, std::forward<Args>(args)...);
    std::cout << message << std::endl;
}

template <typename... Args>
static void exitLog(std::format_string<Args...> fmt, Args&&... args)
{
    std::string message = std::format(fmt, std::forward<Args>(args)...);
    std::cout << message << std::endl;
    assert(false);
    exit(EXIT_FAILURE);
}

}; // namespace guk