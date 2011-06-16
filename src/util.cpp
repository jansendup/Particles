//#include <math.h>
#define GL_INTEROP
#include <stdio.h>
//#include <stdlib.h>
#include <string>
#include <string.h>

#include <CL/cl.h>
#include <CL/cl_gl.h>
#include "util.h"

/*#ifdef _WIN32
#  define WINDOWS_LEAN_AND_MEAN
#  define NOMINMAX
#  include <windows.h>
#endif*/

// OpenGL Graphics Includes
#include <GL/glew.h>
#if defined (__APPLE__) || defined(MACOSX)
#include <OpenGL/OpenGL.h>
#include <GLUT/glut.h>
#else
#include <GL/freeglut.h>
#ifdef UNIX
#include <GL/glx.h>
#endif
#endif

#if defined (__APPLE__) || defined(MACOSX)
#define GL_SHARING_EXTENSION "cl_APPLE_gl_sharing"
#else
#define GL_SHARING_EXTENSION "cl_khr_gl_sharing"
#endif

#if defined __APPLE__ || defined(MACOSX)
#else
#if defined WIN32
#else
//needed for context sharing functions
#include <GL/glx.h>
#endif
#endif

void createContextErrorFeedback(const char* errinfo, const void *private_info, size_t cb, void* user_data);

char *read_file(const char *filename, int *length)
{
	FILE *f = fopen(filename, "r");
	void *buffer;

	if (!f) {
		fprintf(stderr, "Unable to open %s for reading\n", filename);
		return NULL;
	}

	fseek(f, 0, SEEK_END);
	*length = ftell(f);
	fseek(f, 0, SEEK_SET);

	buffer = malloc(*length+1);
	*length = fread(buffer, 1, *length, f);
	fclose(f);
	((char*)buffer)[*length] = '\0';

	return (char*)buffer;
}


bool oclGetNVIDIAPlatform(cl_platform_id* clSelectedPlatformID)
{
	char chBuffer[1024];
	cl_uint num_platforms;
	cl_platform_id* clPlatformIDs;
	cl_int ciErrNum;
	*clSelectedPlatformID = NULL;
	cl_uint i = 0;

	// Get OpenCL platform count
	ciErrNum = clGetPlatformIDs (0, NULL, &num_platforms);
	if (ciErrNum != CL_SUCCESS)
	{
		printf(" Error %i in clGetPlatformIDs Call !!!\n\n", ciErrNum);
		return false;
	}
	else
	{
		if(num_platforms == 0)
		{
			printf("No OpenCL platform found!\n\n");
			return false;
		}
		else
		{
			// if there's a platform or more, make space for ID's
			if ((clPlatformIDs = (cl_platform_id*)malloc(num_platforms * sizeof(cl_platform_id))) == NULL)
			{
				printf("Failed to allocate memory for cl_platform ID's!\n\n");
				return false;
			}

			// get platform info for each platform and trap the NVIDIA platform if found
			ciErrNum = clGetPlatformIDs (num_platforms, clPlatformIDs, NULL);
			printf("Available platforms:\n");
			for(i = 0; i < num_platforms; ++i)
			{
				ciErrNum = clGetPlatformInfo (clPlatformIDs[i], CL_PLATFORM_NAME, 1024, &chBuffer, NULL);
				if(ciErrNum == CL_SUCCESS)
				{
					printf("platform %d: %s\n", i, chBuffer);
					if(strstr(chBuffer, "NVIDIA") != NULL)
					{
						printf("selected platform %d\n", i);
						*clSelectedPlatformID = clPlatformIDs[i];
						break;
					}
				}
			}

			// default to zeroeth platform if NVIDIA not found
			if(*clSelectedPlatformID == NULL)
			{
				//printf("WARNING: NVIDIA OpenCL platform not found - defaulting to first platform!\n\n");
				printf("selected platform: %d\n", 0);
				*clSelectedPlatformID = clPlatformIDs[0];
			}

			free(clPlatformIDs);
		}
	}

	return true;
}

bool oclGetSomeGPUDevice(cl_device_id* deviceId , cl_platform_id platformId, bool glSharing)
{
	cl_uint deviceCount;
	cl_int error;
	cl_device_id* devices;

	// Get number of devices
	error = clGetDeviceIDs(platformId,CL_DEVICE_TYPE_GPU,0, NULL, &deviceCount);
	if(error != CL_SUCCESS)
	{
		printf("Failed to fetch device count with error code %d (%s)\n",error, oclErrorString(error));
		return false;
	}
	if(deviceCount == 0)
	{
		printf("No GPU devices found on system\n");
		return false;
	}

	// Get list of devices
	devices = (cl_device_id*)malloc(deviceCount*sizeof(cl_device_id));
	error = clGetDeviceIDs(platformId,CL_DEVICE_TYPE_GPU, deviceCount, devices, NULL);
	if(error != CL_SUCCESS)
	{
		printf("Failed get list of devices with error code %d (%s)\n",error, oclErrorString(error));
		return false;
	}

	if(glSharing)
	{
		// Search for device that supports context sharing.
		bool foundDevice = false;
		int deviceIndex;
		for(int i = 0; i < deviceCount; i++)
		{
			size_t extensionsSize;

			// Get size of extensions
			error = clGetDeviceInfo(devices[i], CL_DEVICE_EXTENSIONS, 0, NULL, &extensionsSize);
			if(error != CL_SUCCESS)
			{
				printf("Failed to get device extensions size with error code %d (%s)\n",error, oclErrorString(error));
				continue;
			}

			// Get extensions
			char* extensions = (char*)malloc(extensionsSize);
			error = clGetDeviceInfo(devices[i], CL_DEVICE_EXTENSIONS, extensionsSize, extensions, NULL);
			if(error != CL_SUCCESS)
			{
				printf("Failed to get device extensions with error code %d (%s)\n",error, oclErrorString(error));
				free(extensions);
				continue;
			}

			// Check if the extensions contains the GL_SHARING_EXTENSION
			if( strstr(extensions,GL_SHARING_EXTENSION) != NULL )
			{
				foundDevice = true;
				deviceIndex = i;
				free(extensions);
				break;
			}
			free(extensions);
		}

		if(!foundDevice)
		{
			printf("Couldn't find a GPU device supporting \"%s\"\n", GL_SHARING_EXTENSION);
			return false;
		}

		*deviceId = devices[deviceIndex];
	}
	else
	{
		*deviceId = devices[0];
	}

	free(devices);
	return true;
}

bool oclCreateSomeContext(cl_context* context , cl_device_id deviceId,cl_platform_id platformId, bool glSharing)
{
	cl_int error = 0;
	if(glSharing)
	{
		// Define OS-specific context properties and create the OpenCL context
#if defined (__APPLE__)
		CGLContextObj kCGLContext = CGLGetCurrentContext();
		CGLShareGroupObj kCGLShareGroup = CGLGetShareGroup(kCGLContext);
		if( kCGLContext == NULL)
			printf("CGLGetCurrentContext() returned NULL\n");
		if( kCGLShareGroup == NULL)
			printf("CGLGetShareGroup(kCGLContext) returned NULL\n");
		cl_context_properties props[] = 
		{
			CL_CONTEXT_PROPERTY_USE_CGL_SHAREGROUP_APPLE, (cl_context_properties)kCGLShareGroup, 
			0 
		};
		cxGPUContext = clCreateContext(props, 0,0, NULL, NULL, &ciErrNum);
#else
#ifdef UNIX
		GLXContext glxContext = glXGetCurrentContext();
		Display* display = glXGetCurrentDisplay();
		if(glxContext == NULL)
			printf("glXGetCurrentContext() returned NULL\n");
		if(display == NULL)
			printf("glXGetCurrentDisplay() returned NULL\n");
		cl_context_properties props[] = 
		{
			CL_GL_CONTEXT_KHR, (cl_context_properties)glxContext, 
			CL_GLX_DISPLAY_KHR, (cl_context_properties)display, 
			CL_CONTEXT_PLATFORM, (cl_context_properties)cpPlatform, 
			0
		};
		cxGPUContext = clCreateContext(props, 1, &cdDevices[uiDeviceUsed], NULL, NULL, &ciErrNum);
#else // Win32
		HGLRC wglContext = wglGetCurrentContext();
		HDC wglDC = wglGetCurrentDC();
		if(wglContext == NULL)
			printf("wglGetCurrentContext() returned NULL\n");
		if(wglDC == NULL)
			printf("wglGetCurrentDC() returned NULL\n");
		cl_context_properties props[] = 
		{
			CL_GL_CONTEXT_KHR, (cl_context_properties)wglContext, 
			CL_WGL_HDC_KHR, (cl_context_properties)wglDC, 
			CL_CONTEXT_PLATFORM, (cl_context_properties)platformId, 
			0
		};

		*context = clCreateContext(props, 1, &deviceId, createContextErrorFeedback, NULL, &error);
		if(error != CL_SUCCESS)
		{
			printf("Failed to create shared gl-cl context with error code %d (%s)\n", error, oclErrorString(error));
			return false;
		}
#endif
#endif
	}
	else
	{
		*context = clCreateContext(NULL, 1, &deviceId, NULL, NULL, &error);
		if(error != CL_SUCCESS)
		{
			printf("Failed to create cl context with error code %d (%s)\n", error, oclErrorString(error));
			return false;
		}
	}
	return true;
}

void createContextErrorFeedback(const char* errinfo, const void *private_info, size_t cb, void* user_data)
{
	printf("Notify");
	printf(errinfo);
}


// Helper function to get error string
// *********************************************************************
const char* oclErrorString(cl_int error)
{
	static const char* errorString[] = {
		"CL_SUCCESS",
		"CL_DEVICE_NOT_FOUND",
		"CL_DEVICE_NOT_AVAILABLE",
		"CL_COMPILER_NOT_AVAILABLE",
		"CL_MEM_OBJECT_ALLOCATION_FAILURE",
		"CL_OUT_OF_RESOURCES",
		"CL_OUT_OF_HOST_MEMORY",
		"CL_PROFILING_INFO_NOT_AVAILABLE",
		"CL_MEM_COPY_OVERLAP",
		"CL_IMAGE_FORMAT_MISMATCH",
		"CL_IMAGE_FORMAT_NOT_SUPPORTED",
		"CL_BUILD_PROGRAM_FAILURE",
		"CL_MAP_FAILURE",
		"",
		"",
		"",
		"",
		"",
		"",
		"",
		"",
		"",
		"",
		"",
		"",
		"",
		"",
		"",
		"",
		"",
		"CL_INVALID_VALUE",
		"CL_INVALID_DEVICE_TYPE",
		"CL_INVALID_PLATFORM",
		"CL_INVALID_DEVICE",
		"CL_INVALID_CONTEXT",
		"CL_INVALID_QUEUE_PROPERTIES",
		"CL_INVALID_COMMAND_QUEUE",
		"CL_INVALID_HOST_PTR",
		"CL_INVALID_MEM_OBJECT",
		"CL_INVALID_IMAGE_FORMAT_DESCRIPTOR",
		"CL_INVALID_IMAGE_SIZE",
		"CL_INVALID_SAMPLER",
		"CL_INVALID_BINARY",
		"CL_INVALID_BUILD_OPTIONS",
		"CL_INVALID_PROGRAM",
		"CL_INVALID_PROGRAM_EXECUTABLE",
		"CL_INVALID_KERNEL_NAME",
		"CL_INVALID_KERNEL_DEFINITION",
		"CL_INVALID_KERNEL",
		"CL_INVALID_ARG_INDEX",
		"CL_INVALID_ARG_VALUE",
		"CL_INVALID_ARG_SIZE",
		"CL_INVALID_KERNEL_ARGS",
		"CL_INVALID_WORK_DIMENSION",
		"CL_INVALID_WORK_GROUP_SIZE",
		"CL_INVALID_WORK_ITEM_SIZE",
		"CL_INVALID_GLOBAL_OFFSET",
		"CL_INVALID_EVENT_WAIT_LIST",
		"CL_INVALID_EVENT",
		"CL_INVALID_OPERATION",
		"CL_INVALID_GL_OBJECT",
		"CL_INVALID_BUFFER_SIZE",
		"CL_INVALID_MIP_LEVEL",
		"CL_INVALID_GLOBAL_WORK_SIZE",
	};

	const int errorCount = sizeof(errorString) / sizeof(errorString[0]);

	const int index = -error;

	return (index >= 0 && index < errorCount) ? errorString[index] : "";
}

void oclPrintPlatformInfo(cl_platform_id id)
{
	char chBuffer[1024];
	cl_int error;

	// PROFILE
	error = clGetPlatformInfo (id, CL_PLATFORM_PROFILE, 1024, &chBuffer, NULL);
	if(error == CL_SUCCESS)
	{
		printf("Platform profile: %s\n", chBuffer);
	}
	// VERSION 
	error = clGetPlatformInfo (id, CL_PLATFORM_VERSION, 1024, &chBuffer, NULL);
	if(error == CL_SUCCESS)
	{
		printf("Platform version: %s\n", chBuffer);
	}
	// NAME
	error = clGetPlatformInfo (id, CL_PLATFORM_NAME, 1024, &chBuffer, NULL);
	if(error == CL_SUCCESS)
	{
		printf("Platform name: %s\n", chBuffer);
	}
	// VENDOR
	error = clGetPlatformInfo (id, CL_PLATFORM_VENDOR, 1024, &chBuffer, NULL);
	if(error == CL_SUCCESS)
	{
		printf("Platform vendor: %s\n", chBuffer);
	}
	// EXTENSIONS
	error = clGetPlatformInfo (id, CL_PLATFORM_EXTENSIONS, 1024, &chBuffer, NULL);
	if(error == CL_SUCCESS)
	{
		printf("Platform extensions: %s\n", chBuffer);
	}
}

void oclPrintDeviceInfo(cl_device_id device)
{
	char device_string[1024];
	bool nv_device_attibute_query = false;

	// CL_DEVICE_NAME
	clGetDeviceInfo(device, CL_DEVICE_NAME, sizeof(device_string), &device_string, NULL);
	printf( "  CL_DEVICE_NAME: \t\t\t%s\n", device_string);

	// CL_DEVICE_VENDOR
	clGetDeviceInfo(device, CL_DEVICE_VENDOR, sizeof(device_string), &device_string, NULL);
	printf( "  CL_DEVICE_VENDOR: \t\t\t%s\n", device_string);

	// CL_DRIVER_VERSION
	clGetDeviceInfo(device, CL_DRIVER_VERSION, sizeof(device_string), &device_string, NULL);
	printf( "  CL_DRIVER_VERSION: \t\t\t%s\n", device_string);

	// CL_DEVICE_VERSION
	clGetDeviceInfo(device, CL_DEVICE_VERSION, sizeof(device_string), &device_string, NULL);
	printf( "  CL_DEVICE_VERSION: \t\t\t%s\n", device_string);

#if !defined(__APPLE__) && !defined(__MACOSX)
	// CL_DEVICE_OPENCL_C_VERSION (if CL_DEVICE_VERSION version > 1.0)
	if(strncmp("OpenCL 1.0", device_string, 10) != 0) 
	{
		// This code is unused for devices reporting OpenCL 1.0, but a def is needed anyway to allow compilation using v 1.0 headers 
		// This constant isn't #defined in 1.0
#ifndef CL_DEVICE_OPENCL_C_VERSION
#define CL_DEVICE_OPENCL_C_VERSION 0x103D   
#endif

		clGetDeviceInfo(device, CL_DEVICE_OPENCL_C_VERSION, sizeof(device_string), &device_string, NULL);
		printf( "  CL_DEVICE_OPENCL_C_VERSION: \t\t%s\n", device_string);
	}
#endif

	// CL_DEVICE_TYPE
	cl_device_type type;
	clGetDeviceInfo(device, CL_DEVICE_TYPE, sizeof(type), &type, NULL);
	if( type & CL_DEVICE_TYPE_CPU )
		printf( "  CL_DEVICE_TYPE:\t\t\t%s\n", "CL_DEVICE_TYPE_CPU");
	if( type & CL_DEVICE_TYPE_GPU )
		printf( "  CL_DEVICE_TYPE:\t\t\t%s\n", "CL_DEVICE_TYPE_GPU");
	if( type & CL_DEVICE_TYPE_ACCELERATOR )
		printf( "  CL_DEVICE_TYPE:\t\t\t%s\n", "CL_DEVICE_TYPE_ACCELERATOR");
	if( type & CL_DEVICE_TYPE_DEFAULT )
		printf( "  CL_DEVICE_TYPE:\t\t\t%s\n", "CL_DEVICE_TYPE_DEFAULT");

	// CL_DEVICE_MAX_COMPUTE_UNITS
	cl_uint compute_units;
	clGetDeviceInfo(device, CL_DEVICE_MAX_COMPUTE_UNITS, sizeof(compute_units), &compute_units, NULL);
	printf( "  CL_DEVICE_MAX_COMPUTE_UNITS:\t\t%u\n", compute_units);

	// CL_DEVICE_MAX_WORK_ITEM_DIMENSIONS
	size_t workitem_dims;
	clGetDeviceInfo(device, CL_DEVICE_MAX_WORK_ITEM_DIMENSIONS, sizeof(workitem_dims), &workitem_dims, NULL);
	printf( "  CL_DEVICE_MAX_WORK_ITEM_DIMENSIONS:\t%u\n", workitem_dims);

	// CL_DEVICE_MAX_WORK_ITEM_SIZES
	size_t workitem_size[3];
	clGetDeviceInfo(device, CL_DEVICE_MAX_WORK_ITEM_SIZES, sizeof(workitem_size), &workitem_size, NULL);
	printf( "  CL_DEVICE_MAX_WORK_ITEM_SIZES:\t%u / %u / %u \n", workitem_size[0], workitem_size[1], workitem_size[2]);

	// CL_DEVICE_MAX_WORK_GROUP_SIZE
	size_t workgroup_size;
	clGetDeviceInfo(device, CL_DEVICE_MAX_WORK_GROUP_SIZE, sizeof(workgroup_size), &workgroup_size, NULL);
	printf( "  CL_DEVICE_MAX_WORK_GROUP_SIZE:\t%u\n", workgroup_size);

	// CL_DEVICE_MAX_CLOCK_FREQUENCY
	cl_uint clock_frequency;
	clGetDeviceInfo(device, CL_DEVICE_MAX_CLOCK_FREQUENCY, sizeof(clock_frequency), &clock_frequency, NULL);
	printf( "  CL_DEVICE_MAX_CLOCK_FREQUENCY:\t%u MHz\n", clock_frequency);

	// CL_DEVICE_ADDRESS_BITS
	cl_uint addr_bits;
	clGetDeviceInfo(device, CL_DEVICE_ADDRESS_BITS, sizeof(addr_bits), &addr_bits, NULL);
	printf( "  CL_DEVICE_ADDRESS_BITS:\t\t%u\n", addr_bits);

	// CL_DEVICE_MAX_MEM_ALLOC_SIZE
	cl_ulong max_mem_alloc_size;
	clGetDeviceInfo(device, CL_DEVICE_MAX_MEM_ALLOC_SIZE, sizeof(max_mem_alloc_size), &max_mem_alloc_size, NULL);
	printf( "  CL_DEVICE_MAX_MEM_ALLOC_SIZE:\t\t%u MByte\n", (unsigned int)(max_mem_alloc_size / (1024 * 1024)));

	// CL_DEVICE_GLOBAL_MEM_SIZE
	cl_ulong mem_size;
	clGetDeviceInfo(device, CL_DEVICE_GLOBAL_MEM_SIZE, sizeof(mem_size), &mem_size, NULL);
	printf( "  CL_DEVICE_GLOBAL_MEM_SIZE:\t\t%u MByte\n", (unsigned int)(mem_size / (1024 * 1024)));

	// CL_DEVICE_ERROR_CORRECTION_SUPPORT
	cl_bool error_correction_support;
	clGetDeviceInfo(device, CL_DEVICE_ERROR_CORRECTION_SUPPORT, sizeof(error_correction_support), &error_correction_support, NULL);
	printf( "  CL_DEVICE_ERROR_CORRECTION_SUPPORT:\t%s\n", error_correction_support == CL_TRUE ? "yes" : "no");

	// CL_DEVICE_LOCAL_MEM_TYPE
	cl_device_local_mem_type local_mem_type;
	clGetDeviceInfo(device, CL_DEVICE_LOCAL_MEM_TYPE, sizeof(local_mem_type), &local_mem_type, NULL);
	printf( "  CL_DEVICE_LOCAL_MEM_TYPE:\t\t%s\n", local_mem_type == 1 ? "local" : "global");

	// CL_DEVICE_LOCAL_MEM_SIZE
	clGetDeviceInfo(device, CL_DEVICE_LOCAL_MEM_SIZE, sizeof(mem_size), &mem_size, NULL);
	printf( "  CL_DEVICE_LOCAL_MEM_SIZE:\t\t%u KByte\n", (unsigned int)(mem_size / 1024));

	// CL_DEVICE_MAX_CONSTANT_BUFFER_SIZE
	clGetDeviceInfo(device, CL_DEVICE_MAX_CONSTANT_BUFFER_SIZE, sizeof(mem_size), &mem_size, NULL);
	printf( "  CL_DEVICE_MAX_CONSTANT_BUFFER_SIZE:\t%u KByte\n", (unsigned int)(mem_size / 1024));

	// CL_DEVICE_QUEUE_PROPERTIES
	cl_command_queue_properties queue_properties;
	clGetDeviceInfo(device, CL_DEVICE_QUEUE_PROPERTIES, sizeof(queue_properties), &queue_properties, NULL);
	if( queue_properties & CL_QUEUE_OUT_OF_ORDER_EXEC_MODE_ENABLE )
		printf( "  CL_DEVICE_QUEUE_PROPERTIES:\t\t%s\n", "CL_QUEUE_OUT_OF_ORDER_EXEC_MODE_ENABLE");    
	if( queue_properties & CL_QUEUE_PROFILING_ENABLE )
		printf( "  CL_DEVICE_QUEUE_PROPERTIES:\t\t%s\n", "CL_QUEUE_PROFILING_ENABLE");

	// CL_DEVICE_IMAGE_SUPPORT
	cl_bool image_support;
	clGetDeviceInfo(device, CL_DEVICE_IMAGE_SUPPORT, sizeof(image_support), &image_support, NULL);
	printf( "  CL_DEVICE_IMAGE_SUPPORT:\t\t%u\n", image_support);

	// CL_DEVICE_MAX_READ_IMAGE_ARGS
	cl_uint max_read_image_args;
	clGetDeviceInfo(device, CL_DEVICE_MAX_READ_IMAGE_ARGS, sizeof(max_read_image_args), &max_read_image_args, NULL);
	printf( "  CL_DEVICE_MAX_READ_IMAGE_ARGS:\t%u\n", max_read_image_args);

	// CL_DEVICE_MAX_WRITE_IMAGE_ARGS
	cl_uint max_write_image_args;
	clGetDeviceInfo(device, CL_DEVICE_MAX_WRITE_IMAGE_ARGS, sizeof(max_write_image_args), &max_write_image_args, NULL);
	printf( "  CL_DEVICE_MAX_WRITE_IMAGE_ARGS:\t%u\n", max_write_image_args);

	// CL_DEVICE_SINGLE_FP_CONFIG
	cl_device_fp_config fp_config;
	clGetDeviceInfo(device, CL_DEVICE_SINGLE_FP_CONFIG, sizeof(cl_device_fp_config), &fp_config, NULL);
	printf( "  CL_DEVICE_SINGLE_FP_CONFIG:\t\t%s%s%s%s%s%s\n",
		fp_config & CL_FP_DENORM ? "denorms " : "",
		fp_config & CL_FP_INF_NAN ? "INF-quietNaNs " : "",
		fp_config & CL_FP_ROUND_TO_NEAREST ? "round-to-nearest " : "",
		fp_config & CL_FP_ROUND_TO_ZERO ? "round-to-zero " : "",
		fp_config & CL_FP_ROUND_TO_INF ? "round-to-inf " : "",
		fp_config & CL_FP_FMA ? "fma " : "");

	// CL_DEVICE_IMAGE2D_MAX_WIDTH, CL_DEVICE_IMAGE2D_MAX_HEIGHT, CL_DEVICE_IMAGE3D_MAX_WIDTH, CL_DEVICE_IMAGE3D_MAX_HEIGHT, CL_DEVICE_IMAGE3D_MAX_DEPTH
	size_t szMaxDims[5];
	printf( "\n  CL_DEVICE_IMAGE <dim>"); 
	clGetDeviceInfo(device, CL_DEVICE_IMAGE2D_MAX_WIDTH, sizeof(size_t), &szMaxDims[0], NULL);
	printf( "\t\t\t2D_MAX_WIDTH\t %u\n", szMaxDims[0]);
	clGetDeviceInfo(device, CL_DEVICE_IMAGE2D_MAX_HEIGHT, sizeof(size_t), &szMaxDims[1], NULL);
	printf( "\t\t\t\t\t2D_MAX_HEIGHT\t %u\n", szMaxDims[1]);
	clGetDeviceInfo(device, CL_DEVICE_IMAGE3D_MAX_WIDTH, sizeof(size_t), &szMaxDims[2], NULL);
	printf( "\t\t\t\t\t3D_MAX_WIDTH\t %u\n", szMaxDims[2]);
	clGetDeviceInfo(device, CL_DEVICE_IMAGE3D_MAX_HEIGHT, sizeof(size_t), &szMaxDims[3], NULL);
	printf( "\t\t\t\t\t3D_MAX_HEIGHT\t %u\n", szMaxDims[3]);
	clGetDeviceInfo(device, CL_DEVICE_IMAGE3D_MAX_DEPTH, sizeof(size_t), &szMaxDims[4], NULL);
	printf( "\t\t\t\t\t3D_MAX_DEPTH\t %u\n", szMaxDims[4]);

	// CL_DEVICE_EXTENSIONS: get device extensions, and if any then parse & log the string onto separate lines
	clGetDeviceInfo(device, CL_DEVICE_EXTENSIONS, sizeof(device_string), &device_string, NULL);
	if (device_string != 0) 
	{
		printf( "\n  CL_DEVICE_EXTENSIONS:");
		std::string stdDevString;
		stdDevString = std::string(device_string);
		size_t szOldPos = 0;
		size_t szSpacePos = stdDevString.find(' ', szOldPos); // extensions string is space delimited
		while (szSpacePos != stdDevString.npos)
		{
			if( strcmp("cl_nv_device_attribute_query", stdDevString.substr(szOldPos, szSpacePos - szOldPos).c_str()) == 0 )
				nv_device_attibute_query = true;

			if (szOldPos > 0)
			{
				printf( "\t\t");
			}
			printf( "\t\t\t%s\n", stdDevString.substr(szOldPos, szSpacePos - szOldPos).c_str());

			do {
				szOldPos = szSpacePos + 1;
				szSpacePos = stdDevString.find(' ', szOldPos);
			} while (szSpacePos == szOldPos);
		}
		printf( "\n");
	}
	else 
	{
		printf( "  CL_DEVICE_EXTENSIONS: None\n");
	}

	// CL_DEVICE_PREFERRED_VECTOR_WIDTH_<type>
	printf( "  CL_DEVICE_PREFERRED_VECTOR_WIDTH_<t>\t"); 
	cl_uint vec_width [6];
	clGetDeviceInfo(device, CL_DEVICE_PREFERRED_VECTOR_WIDTH_CHAR, sizeof(cl_uint), &vec_width[0], NULL);
	clGetDeviceInfo(device, CL_DEVICE_PREFERRED_VECTOR_WIDTH_SHORT, sizeof(cl_uint), &vec_width[1], NULL);
	clGetDeviceInfo(device, CL_DEVICE_PREFERRED_VECTOR_WIDTH_INT, sizeof(cl_uint), &vec_width[2], NULL);
	clGetDeviceInfo(device, CL_DEVICE_PREFERRED_VECTOR_WIDTH_LONG, sizeof(cl_uint), &vec_width[3], NULL);
	clGetDeviceInfo(device, CL_DEVICE_PREFERRED_VECTOR_WIDTH_FLOAT, sizeof(cl_uint), &vec_width[4], NULL);
	clGetDeviceInfo(device, CL_DEVICE_PREFERRED_VECTOR_WIDTH_DOUBLE, sizeof(cl_uint), &vec_width[5], NULL);
	printf( "CHAR %u, SHORT %u, INT %u, LONG %u, FLOAT %u, DOUBLE %u\n\n\n", 
		vec_width[0], vec_width[1], vec_width[2], vec_width[3], vec_width[4], vec_width[5]); 
}

