#include <CL/cl.h>
#include <CL/cl_gl.h>
#include <stdio.h>
#include <malloc.h>

#include "opengl.h"
#include "OCL.h"
#define UTIL_GL_SHARING
#include "util.h"

#define NUM_PARTICLES 10000

OCL::OCL(void)
{
	initialized = false;
	platformId = 0;
	deviceId = 0;
	context = 0;
	commandQueue = 0;
	program = 0;
	
	cl_static_pos = cl_static_vel = 0;

	cl_velocities = 0;
	vbo_pos = vbo_color = 0;
}


OCL::~OCL(void)
{
	if(initialized)
	{
		if(context)
			clReleaseContext(context);
		if(commandQueue)
			clReleaseCommandQueue(commandQueue);
		if(program)
			clReleaseProgram(program);
		if(kernel)
			clReleaseKernel(kernel);
		if(cl_static_pos)
			clReleaseMemObject(cl_static_pos);
		if(cl_static_vel)
			clReleaseMemObject(cl_static_vel);
		if(cl_velocities)
			clReleaseMemObject(cl_velocities);
		if(vbo_pos)
			glDeleteBuffers(1, &vbo_pos);
		if(vbo_color)
			glDeleteBuffers(1, &vbo_pos);
	}
}

bool OCL::InitializeContext()
{
	cl_int error;
	printf("Initializing OpenCL context...\n");

	if( !oclGetNVIDIAPlatform(&platformId) )
	{
		printf("Failed to get a platform\n");
		return false;
	}
	printf("Got platform...\n");
	oclPrintPlatformInfo(platformId);

	if( !oclGetSomeGPUDevice(&deviceId, platformId, false) )
	{
		printf("Failed to get a GPU device\n");
		return false;
	}
	printf("Got GPU device...\n");
	oclPrintDeviceInfo(deviceId);

	if( !oclCreateSomeContext(&context, deviceId, platformId, false) )
	{
		printf("Failed to create cl context\n");
		return false;
	}
	printf("Created cl context\n");

	commandQueue = clCreateCommandQueue(context,deviceId,NULL, &error);
	if(error != CL_SUCCESS)
	{
		clReleaseContext(context);
		printf("Failed to create command queue with error code %d (%s)",error, oclErrorString(error));
		return false;
	}
	printf("Created command queue.\n");

	initialized = true;
	return true;
}

bool OCL::LoadProgram(const char* file)
{
	printf("Loading OpenCL program...\n");
	cl_int error;
	int length;
	char* read = read_file(file, &length);

	if(!initialized)
	{
		printf("Failed to load program. OpenCL context not initialized.\n");
		return false;
	}

	if(length <= 0)
	{
		printf("Could not read \"");
		printf(file);
		printf("\"\n");
		return false;
	}

	program = clCreateProgramWithSource(context,1,(const char**)&read, (size_t*)&length, &error);
	if(error != CL_SUCCESS)
	{
		printf("Create program of \"");
		printf(file);
		printf("\" faild.\n");
		return false;
	}

	if ( !BuildExecutable() )
	{
		printf("Failed to BuildExecutable OpenCL source.\n");
		return false;
	}

	free(read);

	return true;
}

bool OCL::LoadData(Vector4* pos, Vector4* vel, Vector4* col, int size)
{
	cl_int error;
	printf("Loading data...\n");
	if(!initialized)
	{
		printf("Failed to load data. OpenCL context not initialized. \n");
		return false;
	}
	int buffersSize = sizeof(Vector4) * size;

	printf("Creating OpenGL buffers...\n");
	vbo_pos = oglCreateVBO(pos, buffersSize, GL_ARRAY_BUFFER, GL_DYNAMIC_DRAW);
	if(!vbo_pos)
	{
		printf("Failed to create positions vbo.\n");
		return false;
	}
	vbo_color = oglCreateVBO(col, buffersSize, GL_ARRAY_BUFFER, GL_DYNAMIC_DRAW);
	if(!vbo_color)
	{
		printf("Failed to create colors vbo.\n");
		return false;
	}
	glFinish(); // Wait for gl opperations to finish.

	// Create referances of the OpenGL buffers
	printf("Referencing OpenGL buffers to OpenCL buffers...\n");
	
	cl_glReferances[0] = clCreateFromGLBuffer(context,CL_MEM_READ_WRITE,vbo_pos,&error);
	if(error != CL_SUCCESS)
	{
		printf("Failed to referance gl buffer with error code %d(%s)\n", error, oclErrorString(error));
		return false;
	}
	cl_glReferances[1] = clCreateFromGLBuffer(context,CL_MEM_READ_WRITE,vbo_color,&error);
	if(error != CL_SUCCESS)
	{
		printf("Failed to referance gl buffer with error code %d(%s)\n", error, oclErrorString(error));
		return false;
	}

	
}

bool OCL::BuildExecutable()
{
	cl_int error;

	// Build program
	printf("Building OpenCL program...\n");
	error = clBuildProgram(program, 1, &deviceId, NULL, NULL, NULL);
	
	if(error != CL_SUCCESS)
	{
		printf("Failed to build executable with error code %d (%s)", error, oclErrorString(error));
		return false;
	}

	// Get and print build status messages.
	cl_build_status build_status;
	error = clGetProgramBuildInfo(program, deviceId, CL_PROGRAM_BUILD_STATUS, sizeof(cl_build_status), &build_status, NULL);

	char *build_log;
	size_t ret_val_size;
	error = clGetProgramBuildInfo(program, deviceId, CL_PROGRAM_BUILD_LOG, 0, NULL, &ret_val_size);

	build_log = new char[ret_val_size+1];
	error = clGetProgramBuildInfo(program, deviceId, CL_PROGRAM_BUILD_LOG, ret_val_size, build_log, NULL);
	build_log[ret_val_size] = '\0';
	printf("BUILD LOG: \n %s", build_log);

	delete build_log;
	return true;
}

bool OCL::Run()
{
	cl_int error;
	printf("Runnig program on GPU...\n");

	if(!initialized)
	{
		printf("Failed to run. OpenCL context not initialized.\n");
		return false;
	}

	// Create kernel
	kernel = clCreateKernel(program, "Test", &error);

	if(error != CL_SUCCESS)
	{
		printf("Failed to create kernel with error code %d(%s)", error, oclErrorString(error));
		return false;
	}

	// Create arrays
	float *a = new float [num];
	float *b = new float [num];
	for(int i = 0; i < num; i++)
	{
		a[i] = i;
		b[i] = i;
	}

	// Create buffers
	cl_a = clCreateBuffer(context,CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR, sizeof(float) *num, a, &error);
	if(error != CL_SUCCESS)
	{
		printf("Failed to create buffer a with error code %d(%s)\n",error, oclErrorString(error));
		return false;
	}
	cl_b = clCreateBuffer(context,CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR, sizeof(float) *num, a, &error);
	if(error != CL_SUCCESS)
	{
		printf("Failed to create buffer b with error code %d(%s)\n",error, oclErrorString(error));
		return false;
	}
	cl_c = clCreateBuffer(context,CL_MEM_WRITE_ONLY | CL_MEM_COPY_HOST_PTR, sizeof(float) *num, a, &error);
	if(error != CL_SUCCESS)
	{
		printf("Failed to create buffer c with error code %d(%s)\n",error, oclErrorString(error));
		return false;
	}

	// Set kernel arguments
	error = clSetKernelArg(kernel, 0, sizeof(cl_mem), (void*)&cl_a);
	if(error != CL_SUCCESS)
	{
		printf("Failed to set kernel arguments with error code %d(%s)\n",error, oclErrorString(error));
		return false;
	}
	error = clSetKernelArg(kernel, 1, sizeof(cl_mem), (void*)&cl_b);
	if(error != CL_SUCCESS)
	{
		printf("Failed to set kernel arguments with error code %d(%s)\n",error, oclErrorString(error));
		return false;
	}
	error = clSetKernelArg(kernel, 2, sizeof(cl_mem), (void*)&cl_c);
	if(error != CL_SUCCESS)
	{
		printf("Failed to set kernel arguments with error code %d(%s)\n",error, oclErrorString(error));
		return false;
	}

	// Makes sure queue is empty
	clFinish(commandQueue);

	cl_event event;
	error = clEnqueueNDRangeKernel(commandQueue,kernel,1,NULL,(size_t*)&num,NULL,0,NULL, &event);
	if(error != CL_SUCCESS)
	{
		printf("Failed to execute kernel with error code %d(%s)\n",error, oclErrorString(error));
		return false;
	}
	clReleaseEvent(event);

	float* c_done = new float [num];
	error = clEnqueueReadBuffer(commandQueue,cl_c,CL_TRUE,0, sizeof(float) * num, c_done, 0, NULL, &event);
	if(error != CL_SUCCESS)
	{
		printf("Failed to read buffer c with error code %d(%s)\n",error, oclErrorString(error));
		return false;
	}
	clReleaseEvent(event);

	for(int i = 0; i < num; i++)
	{
		printf("c_done[%d] = %g\n",i, c_done[i]);
	}

	delete[] a;
	delete[] b;
	delete[] c_done;
	return true;
}