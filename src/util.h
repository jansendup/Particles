#ifndef UTIL_H
#define UTIL_H

char *read_file(const char *filename, int *length);

#ifdef UTIL_GL_SHARING
#include "opengl.h"
GLuint oglCreateVBO(const void* data, int dataSize, GLenum target, GLenum usage);
#endif

bool oclGetNVIDIAPlatform(cl_platform_id* clSelectedPlatformID);
bool oclGetSomeGPUDevice(cl_device_id* deviceId , cl_platform_id platformId);
bool oclCreateSomeContext(cl_context* context , cl_device_id deviceId,cl_platform_id platformId);

const char* oclErrorString(cl_int error);
void oclPrintPlatformInfo(cl_platform_id id);
void oclPrintDeviceInfo(cl_device_id device);


#endif
