#ifndef PTI_TOOLS_UNITRACE_VERSION_H_
#define PTI_TOOLS_UNITRACE_VERSION_H_

#define UNITRACE_VERSION	"2.1.0"

std::string get_version();

#if !defined(_WIN32) && (defined(__gnu_linux__) || defined(__unix__))
#define LIB_UNITRACE_TOOL_NAME	"libunitrace_tool.so"
#else /* !defined(_WIN32) && (defined(__gnu_linux__) || defined(__unix__)) */
#define LIB_UNITRACE_TOOL_NAME	"unitrace_tool.dll"
#endif /* !defined(_WIN32) && (defined(__gnu_linux__) || defined(__unix__)) */

#endif /* PTI_TOOLS_UNITRACE_VERSION_H_ */
