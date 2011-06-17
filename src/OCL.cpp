#include <stdio.h>
#include <malloc.h>

#include "opengl.h"
#include "OCL.h"
#include "util.h"
#include <CL/cl.h>
#include <CL/cl_gl.h>

OCL::OCL(void)
{
	initialized = false;
	platformId = 0;
	deviceId = 0;
	context = 0;
	commandQueue = 0;
	program = 0;
	
	buffersSize = 0;

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

	if( !oclGetSomeGPUDevice(&deviceId, platformId) )
	{
		printf("Failed to get a GPU device\n");
		return false;
	}
	printf("Got GPU device...\n");
	oclPrintDeviceInfo(deviceId);

	if( !oclCreateSomeContext(&context, deviceId, platformId) )
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
	buffersSize = sizeof(Vector4) * size;
	printf("Sizeof(Vector4) = %d\n", sizeof(Vector4));

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

	cl_velocities = clCreateBuffer(context, CL_MEM_READ_WRITE, buffersSize, NULL, &error);
	if(error != CL_SUCCESS)
	{
		printf("Failed to create cl buffer with error code %d(%s)\n", error, oclErrorString(error));
		return false;
	}
	cl_static_pos = clCreateBuffer(context, CL_MEM_READ_WRITE, buffersSize, NULL, &error);
	if(error != CL_SUCCESS)
	{
		printf("Failed to create cl buffer with error code %d(%s)\n", error, oclErrorString(error));
		return false;
	}
	cl_static_vel = clCreateBuffer(context, CL_MEM_READ_WRITE, buffersSize, NULL, &error);
	if(error != CL_SUCCESS)
	{
		printf("Failed to create cl buffer with error code %d(%s)\n", error, oclErrorString(error));
		return false;
	}

	printf("Writing data to GPU memory...\n");

	error = clEnqueueWriteBuffer(commandQueue,cl_velocities, CL_TRUE, 0, buffersSize, vel, 0, NULL, NULL);
	if(error != CL_SUCCESS)
	{
		printf("Failed to write to cl buffer with error code %d(%s)\n", error, oclErrorString(error));
		return false;
	}
	error = clEnqueueWriteBuffer(commandQueue,cl_static_vel, CL_TRUE, 0, buffersSize, vel, 0, NULL, NULL);
	if(error != CL_SUCCESS)
	{
		printf("Failed to write to cl buffer with error code %d(%s)\n", error, oclErrorString(error));
		return false;
	}
	error = clEnqueueWriteBuffer(commandQueue,cl_static_pos, CL_TRUE, 0, buffersSize, pos, 0, NULL, NULL);
	if(error != CL_SUCCESS)
	{
		printf("Failed to write to cl buffer with error code %d(%s)\n", error, oclErrorString(error));
		return false;
	}

	clFinish(commandQueue);
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

bool OCL::CreateKernel()
{
	cl_int error;
	printf("Creating kernel...\n");

	if(!initialized)
	{
		printf("Failed to run. OpenCL context not initialized.\n");
		return false;
	}

	// Create kernel
	kernel = clCreateKernel(program, "updateParticles", &error);

	if(error != CL_SUCCESS)
	{
		printf("Failed to create kernel with error code %d(%s)", error, oclErrorString(error));
		return false;
	}

	// Set kernel arguments
	error = clSetKernelArg(kernel, 0, sizeof(cl_mem), (void*)&cl_glReferances[0]);
	if(error != CL_SUCCESS)
	{
		printf("Failed to set kernel argument 0 with error code %d(%s)\n",error, oclErrorString(error));
		return false;
	}
	error = clSetKernelArg(kernel, 1, sizeof(cl_mem), (void*)&cl_glReferances[1]);
	if(error != CL_SUCCESS)
	{
		printf("Failed to set kernel argument 1 with error code %d(%s)\n",error, oclErrorString(error));
		return false;
	}
	error = clSetKernelArg(kernel, 2, sizeof(cl_mem), (void*)&cl_velocities);
	if(error != CL_SUCCESS)
	{
		printf("Failed to set kernel argument 2 with error code %d(%s)\n",error, oclErrorString(error));
		return false;
	}
	error = clSetKernelArg(kernel, 3, sizeof(cl_mem), (void*)&cl_static_pos);
	if(error != CL_SUCCESS)
	{
		printf("Failed to set kernel arguments 3 with error code %d(%s)\n",error, oclErrorString(error));
		return false;
	}
	error = clSetKernelArg(kernel, 4, sizeof(cl_mem), (void*)&cl_static_vel);
	if(error != CL_SUCCESS)
	{
		printf("Failed to set kernel argument 4 with error code %d(%s)\n",error, oclErrorString(error));
		return false;
	}
	float dt = 0.01f;
	error = clSetKernelArg(kernel, 5, sizeof(float), (void*)&dt);
	if(error != CL_SUCCESS)
	{
		printf("Failed to set kernel argument 5 with error code %d(%s)\n",error, oclErrorString(error));
		return false;
	}
	return true;
}

bool OCL::Run()
{
	cl_int error;

	// Makes sure queue is empty
	glFinish();
	clFinish(commandQueue);
	
	cl_event event;
	error = clEnqueueAcquireGLObjects(commandQueue,2,cl_glReferances,0,NULL,&event);
	if(error != CL_SUCCESS)
	{
		printf("Failed to acquire GL objects with error code %d(%s)\n",error, oclErrorString(error));
		return false;
	}
	clReleaseEvent(event);
	//clFinish(commandQueue);
	size_t s = buffersSize / sizeof(Vector4);
	error = clEnqueueNDRangeKernel(commandQueue,kernel,1,NULL,(size_t*)&s ,NULL,0,NULL, &event); // Another source of problem. Won't allow clEnqueueReleaseGLObjects after excution.
	if(error != CL_SUCCESS)
	{
		printf("Failed to execute kernel with error code %d(%s)\n",error, oclErrorString(error));
		//return false;
	}
	clReleaseEvent(event);
	//clFinish(commandQueue);
	error = clEnqueueReleaseGLObjects(commandQueue,2,cl_glReferances,0, NULL, &event); // Source of problem
	clReleaseEvent(event);
	if(error != CL_SUCCESS)
	{
		printf("Failed to release GL Objects with error code %d(%s)\n",error, oclErrorString(error));
		clFinish(commandQueue);
		return false;
	}

	clFinish(commandQueue);

	return true;
}