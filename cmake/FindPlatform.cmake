if(CMAKE_SIZEOF_VOID_P MATCHES 8)
    set(PLATFORM 64)
    MESSAGE(STATUS "Detected 64-bit platform")
else()
    set(PLATFORM 32)
    MESSAGE(STATUS "Detected 32-bit platform")
endif()

if(PLATFORM MATCHES 32) # 32-bit
    set(DEP_ARCH win32)
else() # 64-bit
    set(DEP_ARCH x64)
endif()

if(XCODE)
  if(PLATFORM MATCHES 32)
    set(CMAKE_OSX_ARCHITECTURES i386)
  else()
    set(CMAKE_OSX_ARCHITECTURES x86_64)
  endif()
endif()