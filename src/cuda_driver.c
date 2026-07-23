#include "cuda_driver.h"

#ifdef _WIN32
#define TokenType WindowsTokenType
#include <windows.h>
#undef TokenType
#else
#include <dlfcn.h>
#endif

/* CUDA Driver API function types */
typedef int (*CuInit_t)(unsigned int);
typedef int (*CuDeviceGet_t)(int*, int);
typedef int (*CuDeviceGetName_t)(char*, int, int);
typedef int (*CuDeviceTotalMem_t)(size_t*, int);
typedef int (*CuDeviceGetAttribute_t)(int*, int, int);
typedef int (*CuDeviceGetCount_t)(int*);

#define CU_DEVICE_ATTRIBUTE_COMPUTE_CAPABILITY_MAJOR 75
#define CU_DEVICE_ATTRIBUTE_COMPUTE_CAPABILITY_MINOR 76

bool cuda_driver_init(CUDADriverInfo* info) {
    memset(info, 0, sizeof(CUDADriverInfo));

#ifdef _WIN32
    HMODULE lib = LoadLibraryA("nvcuda.dll");
    if (!lib) {
        info->is_available = false;
        snprintf(info->device_name, sizeof(info->device_name), "CPU-Only (No CUDA Driver Found)");
        info->total_vram_mb = 0;
        return false;
    }

    /* Resolve real CUDA Driver API functions */
    CuInit_t cuInit = (CuInit_t)GetProcAddress(lib, "cuInit");
    CuDeviceGet_t cuDeviceGet = (CuDeviceGet_t)GetProcAddress(lib, "cuDeviceGet");
    CuDeviceGetName_t cuDeviceGetName = (CuDeviceGetName_t)GetProcAddress(lib, "cuDeviceGetName");
    CuDeviceTotalMem_t cuDeviceTotalMem = (CuDeviceTotalMem_t)GetProcAddress(lib, "cuDeviceTotalMem_v2");
    CuDeviceGetAttribute_t cuDeviceGetAttribute = (CuDeviceGetAttribute_t)GetProcAddress(lib, "cuDeviceGetAttribute");
    CuDeviceGetCount_t cuDeviceGetCount = (CuDeviceGetCount_t)GetProcAddress(lib, "cuDeviceGetCount");

    if (!cuInit || !cuDeviceGet || !cuDeviceGetName || !cuDeviceTotalMem || !cuDeviceGetAttribute || !cuDeviceGetCount) {
        info->is_available = true;
        snprintf(info->device_name, sizeof(info->device_name), "CUDA Driver Loaded (API Query Unavailable)");
        FreeLibrary(lib);
        return true;
    }

    if (cuInit(0) != 0) {
        info->is_available = false;
        snprintf(info->device_name, sizeof(info->device_name), "CUDA Init Failed");
        FreeLibrary(lib);
        return false;
    }

    int device_count = 0;
    cuDeviceGetCount(&device_count);
    if (device_count <= 0) {
        info->is_available = false;
        snprintf(info->device_name, sizeof(info->device_name), "No CUDA Devices Found");
        FreeLibrary(lib);
        return false;
    }

    int device = 0;
    cuDeviceGet(&device, 0);

    /* Query real device name */
    cuDeviceGetName(info->device_name, sizeof(info->device_name), device);

    /* Query real VRAM */
    size_t total_mem = 0;
    cuDeviceTotalMem(&total_mem, device);
    info->total_vram_mb = total_mem / (1024 * 1024);

    /* Query real compute capability */
    int major = 0, minor = 0;
    cuDeviceGetAttribute(&major, CU_DEVICE_ATTRIBUTE_COMPUTE_CAPABILITY_MAJOR, device);
    cuDeviceGetAttribute(&minor, CU_DEVICE_ATTRIBUTE_COMPUTE_CAPABILITY_MINOR, device);
    info->compute_capability_major = major;
    info->compute_capability_minor = minor;

    info->is_available = true;
    FreeLibrary(lib);
    return true;

#else
    void* lib = dlopen("libcuda.so", RTLD_LAZY);
    if (!lib) lib = dlopen("libcuda.so.1", RTLD_LAZY);
    if (!lib) {
        info->is_available = false;
        snprintf(info->device_name, sizeof(info->device_name), "CPU-Only (No CUDA Driver Found)");
        return false;
    }

    CuInit_t cuInit = (CuInit_t)dlsym(lib, "cuInit");
    CuDeviceGet_t cuDeviceGet = (CuDeviceGet_t)dlsym(lib, "cuDeviceGet");
    CuDeviceGetName_t cuDeviceGetName = (CuDeviceGetName_t)dlsym(lib, "cuDeviceGetName");
    CuDeviceTotalMem_t cuDeviceTotalMem = (CuDeviceTotalMem_t)dlsym(lib, "cuDeviceTotalMem_v2");
    CuDeviceGetAttribute_t cuDeviceGetAttribute = (CuDeviceGetAttribute_t)dlsym(lib, "cuDeviceGetAttribute");
    CuDeviceGetCount_t cuDeviceGetCount = (CuDeviceGetCount_t)dlsym(lib, "cuDeviceGetCount");

    if (!cuInit || !cuDeviceGet || !cuDeviceGetName) {
        info->is_available = true;
        snprintf(info->device_name, sizeof(info->device_name), "CUDA Driver Loaded (API Query Unavailable)");
        dlclose(lib);
        return true;
    }

    if (cuInit(0) != 0) {
        info->is_available = false;
        snprintf(info->device_name, sizeof(info->device_name), "CUDA Init Failed");
        dlclose(lib);
        return false;
    }

    int device_count = 0;
    if (cuDeviceGetCount) cuDeviceGetCount(&device_count);
    if (device_count <= 0) {
        info->is_available = false;
        snprintf(info->device_name, sizeof(info->device_name), "No CUDA Devices Found");
        dlclose(lib);
        return false;
    }

    int device = 0;
    cuDeviceGet(&device, 0);
    cuDeviceGetName(info->device_name, sizeof(info->device_name), device);

    if (cuDeviceTotalMem) {
        size_t total_mem = 0;
        cuDeviceTotalMem(&total_mem, device);
        info->total_vram_mb = total_mem / (1024 * 1024);
    }

    if (cuDeviceGetAttribute) {
        int major = 0, minor = 0;
        cuDeviceGetAttribute(&major, CU_DEVICE_ATTRIBUTE_COMPUTE_CAPABILITY_MAJOR, device);
        cuDeviceGetAttribute(&minor, CU_DEVICE_ATTRIBUTE_COMPUTE_CAPABILITY_MINOR, device);
        info->compute_capability_major = major;
        info->compute_capability_minor = minor;
    }

    info->is_available = true;
    dlclose(lib);
    return true;
#endif
}

void cuda_driver_print_status(const CUDADriverInfo* info) {
    if (info->is_available && info->total_vram_mb > 0) {
        printf("[Vex CUDA Driver] GPU: %s (%zu MB VRAM, Compute %d.%d)\n",
            info->device_name, info->total_vram_mb, info->compute_capability_major, info->compute_capability_minor);
    } else if (info->is_available) {
        printf("[Vex CUDA Driver] %s\n", info->device_name);
    } else {
        printf("[Vex Compute] %s — Using CPU Tensor Engine\n", info->device_name);
    }
}
