#ifndef MINIZ_EXPORT_H
#define MINIZ_EXPORT_H

#ifdef MINIZ_DLL
  #ifdef MINIZ_IMPLEMENTATION
    #define MINIZ_EXPORT __declspec(dllexport)
  #else
    #define MINIZ_EXPORT __declspec(dllimport)
  #endif
#else
  #define MINIZ_EXPORT
#endif

#endif
