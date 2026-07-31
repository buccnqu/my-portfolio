// perf.cpp - Full English, supports Python 2.4 ~ 3.15
// Compile: cl /EHsc /openmp /arch:AVX2 /O2 perf.cpp

#define _CRT_SECURE_NO_WARNINGS
#pragma warning(disable: 4996) // 【新增】忽略 GetVersionEx 等函数的过时警告
#include <iostream>
#include <chrono>
#include <thread>
#include <vector>
#include <fstream>
#include <string>
#include <cstdlib>
#include <cstring>
#include <cmath>
#include <omp.h>
#include <immintrin.h>
#include <windows.h>
#include <psapi.h>
#include <intrin.h>

using namespace std;
using namespace chrono;

// ------------------------------------------------------------------
// 1. System Info
// ------------------------------------------------------------------
void system_info() {
    cout << "\n=== SYSTEM INFORMATION ===\n";
    SYSTEM_INFO si;
    GetSystemInfo(&si);
    cout << "CPU logical cores: " << si.dwNumberOfProcessors << "\n";
    HKEY hKey;
    // 【修改】使用 RegOpenKeyExA 替代 RegOpenKeyEx
    if (RegOpenKeyExA(HKEY_LOCAL_MACHINE, "HARDWARE\\DESCRIPTION\\System\\CentralProcessor\\0", 0, KEY_READ, &hKey) == ERROR_SUCCESS) {
        char name[256];
        DWORD size = sizeof(name);
        // 【修改】使用 RegQueryValueExA 替代 RegQueryValueEx
        if (RegQueryValueExA(hKey, "ProcessorNameString", nullptr, nullptr, (LPBYTE)name, &size) == ERROR_SUCCESS) {
            cout << "CPU model: " << name << "\n";
        }
        RegCloseKey(hKey);
    }
    MEMORYSTATUSEX mem;
    mem.dwLength = sizeof(mem);
    GlobalMemoryStatusEx(&mem);
    cout << "Total RAM: " << (mem.ullTotalPhys / (1024ULL * 1024 * 1024)) << " GB\n";

    // 【修改】使用 OSVERSIONINFOEXA 和 GetVersionExA
    OSVERSIONINFOEXA os;
    os.dwOSVersionInfoSize = sizeof(OSVERSIONINFOEXA);
    GetVersionExA((LPOSVERSIONINFOA)&os);
    cout << "OS: Windows " << os.dwMajorVersion << "." << os.dwMinorVersion << " (Build " << os.dwBuildNumber << ")\n";
}

// ------------------------------------------------------------------
// 2. CPU Benchmark (AVX2 + OpenMP)
// ------------------------------------------------------------------
double cpu_gflops() {
    const long long N = 300000000LL;
    volatile double sum = 0.0;
    auto start = high_resolution_clock::now();
#pragma omp parallel for reduction(+:sum)
    for (long long i = 0; i < N; ++i) {
        double x = sin((double)i * 0.001);
        double y = cos((double)i * 0.001);
        sum += x * y + 0.001;
    }
    auto end = high_resolution_clock::now();
    double elapsed = duration<double>(end - start).count();
    return (double)N / elapsed / 1e9;  // GFLOPS
}

// ------------------------------------------------------------------
// 3. Memory Bandwidth (GB/s)
// ------------------------------------------------------------------
double memory_gbps(int sizeMB = 512) {
    size_t size = (size_t)sizeMB * 1024 * 1024;
    char* p = (char*)VirtualAlloc(nullptr, size, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    if (!p) return 0.0;
    auto start = high_resolution_clock::now();
    for (size_t i = 0; i < size; ++i) p[i] = (char)(i & 0xFF);
    volatile char sum = 0;
    for (size_t i = 0; i < size; ++i) sum += p[i];
    auto end = high_resolution_clock::now();
    VirtualFree(p, 0, MEM_RELEASE);
    double elapsed = duration<double>(end - start).count();
    return (sizeMB / 1024.0) / elapsed;  // GB/s
}

// ------------------------------------------------------------------
// 4. Disk Throughput (GB/s)
// ------------------------------------------------------------------
double disk_gbps() {
    const char* fname = "disk_test.tmp";
    const int MB = 1024;
    const int bufSize = 1 * 1024 * 1024;
    vector<char> buf(bufSize, 'A');
    auto start = high_resolution_clock::now();
    ofstream ofs(fname, ios::binary);
    for (int i = 0; i < MB; ++i) ofs.write(buf.data(), bufSize);
    ofs.close();
    ifstream ifs(fname, ios::binary);
    vector<char> readBuf(bufSize);
    while (ifs.read(readBuf.data(), bufSize)) {}
    ifs.close();
    auto end = high_resolution_clock::now();
    remove(fname);
    double elapsed = duration<double>(end - start).count();
    return (MB / 1024.0) / elapsed;  // GB/s
}

// ------------------------------------------------------------------
// 5. Python Runtime Evolution (compatible with 2.4 ~ 3.15)
// ------------------------------------------------------------------
void python_benchmark() {
    cout << "\n=== PYTHON RUNTIME EVOLUTION (2.4 -> 3.15) ===\n";

    // 定义版本及其对应的可执行文件路径
    struct PyVersion {
        const char* ver;
        const char* exe_path; // exe_path 为空则使用 py -x.x
    };

    PyVersion versions[] = {
        {"2.4", "C:/python24/python.exe"},
        {"2.7", "C:/python27/python.exe"},
        {"3.9", "py -3.9"},
        {"3.10", "py -3.10"},
        {"3.11", "py -3.11"},
        {"3.12", "py -3.12"},
        {"3.13", "py -3.13"},
        {"3.14", "py -3.14"},
        {"3.15", "py -3.15"}
    };

    for (const auto& ver_ctx : versions) {
        // 构建命令：如果是绝对路径直接调用，否则用 py 启动器
        string cmd;
        if (strlen(ver_ctx.exe_path) > 2 && strstr(ver_ctx.exe_path, "python.exe")) {
            cmd = string(ver_ctx.exe_path); // 绝对路径，如 C:/python24/python.exe
        }
        else {
            cmd = string(ver_ctx.exe_path); // py启动器，如 py -3.9 （【关键修改】这里删掉了后面加的版本号）
        }

        // 拼接 Python 测试代码 (完美兼容 Python 2.4 - 3.15)
        cmd += " -c \"import time, math, sys; s=time.time(); "
            "sum(math.sqrt(i) for i in range(3000000)); "
            "sys.stdout.write('Python ' + '" + string(ver_ctx.ver) + "' + ' -> ' + str(round(time.time()-s, 3)) + 's\\n')\"";

        cout << "Running " << ver_ctx.ver << "...\n";
        int ret = system(cmd.c_str());
        if (ret != 0) {
            cout << "  (version " << ver_ctx.ver << " not available or failed. Path: " << ver_ctx.exe_path << ")\n";
        }
    }
}

// ------------------------------------------------------------------
// JSON report
// ------------------------------------------------------------------
void json_report(double cpu, double mem, double disk) {
    cout << "\n{\n";
    cout << "  \"cpu_gflops\": " << cpu << ",\n";
    cout << "  \"memory_gbps\": " << mem << ",\n";
    cout << "  \"disk_gbps\": " << disk << "\n";
    cout << "}\n";
}

// ------------------------------------------------------------------
// Main
// ------------------------------------------------------------------
int main(int argc, char* argv[]) {
    bool jsonMode = false;
    for (int i = 1; i < argc; ++i)
        if (strcmp(argv[i], "--json") == 0) jsonMode = true;

    if (jsonMode) {
        json_report(cpu_gflops(), memory_gbps(), disk_gbps());
        return 0;
    }

    cout << "=== Performance Benchmark Suite ===\n";
    cout << "1. System Info\n2. CPU (GFLOPS)\n3. Memory (GB/s)\n";
    cout << "4. Disk (GB/s)\n5. Python Evolution\n6. Full Test\n7. Exit\n";
    cout << "Select: ";
    int ch; cin >> ch;

    switch (ch) {
    case 1: system_info(); break;
    case 2: cout << "CPU GFLOPS: " << cpu_gflops() << "\n"; break;
    case 3: cout << "Memory GB/s: " << memory_gbps() << "\n"; break;
    case 4: cout << "Disk GB/s: " << disk_gbps() << "\n"; break;
    case 5: python_benchmark(); break;
    case 6: {
        double c = cpu_gflops(), m = memory_gbps(), d = disk_gbps();
        cout << "CPU: " << c << " GFLOPS\nMemory: " << m << " GB/s\nDisk: " << d << " GB/s\n";
        python_benchmark();
        break;
    }
    default: return 0;
    }
    return 0;
}