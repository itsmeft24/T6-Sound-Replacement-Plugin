#pragma once
#include <Windows.h>
#include <algorithm>
#include <iostream>

const inline void make_lowercase(std::string &str) {
  std::transform(str.begin(), str.end(), str.begin(),
                 [](unsigned char c) { return std::tolower(c); });
}
const inline void make_uppercase(std::string &str) {
  std::transform(str.begin(), str.end(), str.begin(),
                 [](unsigned char c) { return std::toupper(c); });
}

bool SetExecuteReadWritePermission(void *ptr, size_t sz);

bool SetReadWritePermission(void *ptr, size_t sz);

void WriteJMP(void *src, void *dst);

void WriteCALL(void *src, void *dst);

void WritePUSH(void *src, void *dst);

bool WriteNOP(void *addr, size_t code_size);

#define AllocateCode(size)                                                     \
  (char *)VirtualAlloc(NULL, size, MEM_COMMIT | MEM_RESERVE,                   \
                       PAGE_EXECUTE_READWRITE);

#define FreeCode(ptr) (bool)VirtualFree(ptr, 0, MEM_RELEASE);