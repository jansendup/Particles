#pragma once
#include <CL/cl.h>

class OCL
{
public:
	OCL(void);
	~OCL(void);

	bool InitializeContext();
	bool LoadProgram(const char* file);
	bool Run();
private:
	bool BuildExecutable();

	cl_platform_id platformId;
	cl_device_id deviceId;
	cl_context context;
	cl_command_queue commandQueue;
	cl_program program;
	cl_kernel kernel;
	cl_mem cl_a, cl_b, cl_c;

	bool initialized;
};

