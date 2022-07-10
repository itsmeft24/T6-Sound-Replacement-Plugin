
#include "Utils.h"

bool SetExecuteReadWritePermission(void *ptr, size_t sz) {
  DWORD oldProtect;
  return (bool)VirtualProtect(ptr, sz, PAGE_EXECUTE_READWRITE, &oldProtect);
}

bool SetReadWritePermission(void *ptr, size_t sz) {
  DWORD oldProtect;
  return (bool)VirtualProtect(ptr, sz, PAGE_READWRITE, &oldProtect);
}

void WriteJMP(void *src, void *dst) {
  DWORD relativeAddress = (DWORD)((unsigned char *)dst - (DWORD)src) - 5;

  SetExecuteReadWritePermission(src, 5);
  *(unsigned char *)src = 0xE9;
  *(DWORD *)((DWORD)src + 1) = relativeAddress;
}

void WriteCALL(void *src, void *dst) {
  DWORD relativeAddress = (DWORD)((unsigned char *)dst - (DWORD)src) - 5;

  SetExecuteReadWritePermission(src, 5);
  *(unsigned char *)src = 0xE8;
  *(DWORD *)((DWORD)src + 1) = relativeAddress;
}

void WritePUSH(void *src, void *dst) {
  SetExecuteReadWritePermission(src, 5);
  *(unsigned char *)src = 0x68;
  *(DWORD *)((DWORD)src + 1) = (DWORD)dst;
}

bool WriteNOP(void *addr, size_t code_size) {
  if (SetExecuteReadWritePermission(addr, code_size)) {
    memset(addr, 0x90, code_size);
    return true;
  }
  return false;
}
