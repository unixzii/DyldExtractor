/*
 * MIT License
 *
 * Copyright (c) 2021 Cyandev<unixzii@gmail.com>. All rights reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#include <dlfcn.h>
#include <unistd.h>
#include <dispatch/dispatch.h>

#include <iostream>
#include <string>

static const char * const prog_version = "1.15.0";
static const char * const default_extractor_path = "/usr/lib/dsc_extractor.bundle";

static const char *g_cmdline;
static bool g_verbose = false;

static void printUsage() {
    std::cout << "OVERVIEW: dyld shared cache extractor (" << prog_version << ")\n\n";
    std::cout << "USAGE: " << g_cmdline << " <cache file> [-v] [-d arg] [-l arg]\n\n";
    std::cout << "OPTIONS:\n";
    std::cout << "\t-v print verbose messages\n";
    std::cout << "\t-d set destination path for the extracted files\n";
    std::cout << "\t-l set path to dsc_extractor library\n";
    std::cout << std::endl;
}

static void printError(const std::string &error) {
    std::cout << "error: " << g_cmdline << ": " << error << std::endl;
}

static void printVerbose(const std::string &str) {
    if (g_verbose) {
        std::cout << "[!] " << str << std::endl;
    }
}

struct ErrorReporting {
    inline bool hasError() const { return _has_error; }
    inline const std::string &error() const { return _error; }
protected:
    bool _has_error = true;
    std::string _error;
};

struct Options : public ErrorReporting {
    explicit Options(int argc, const char *argv[]) : argc(argc), argv(argv) {
        if (argc < 2) {
            _error = "the cache file must be specified";
            return;
        }
        
        _input_path = argv[1];
        
        for (int i = 2; i < argc; ++i) {
            std::string opt = argv[i];
            if (opt == "-v") {
                _verbose = true;
            } else if (opt == "-d") {
                if (!getOptionValue(i, opt, _dest_path, _has_dest_path)) {
                    return;
                }
            } else if (opt == "-l") {
                if (!getOptionValue(i, opt, _library_path, _has_library_path)) {
                    return;
                }
            } else {
                _error = "unknown option: " + opt;
                return;
            }
        }
        
        // Parsed ok.
        _has_error = false;
    }
    
    inline const std::string &inputPath() const { return _input_path; }
    
    inline bool verbose() const { return _verbose; }
    
    inline bool hasDestPath() const { return _has_dest_path; }
    inline const std::string &destPath() const { return _dest_path; }
    
    inline bool hasLibraryPath() const { return _has_library_path; }
    inline const std::string &libraryPath() const { return _library_path; }
    
private:
    bool getOptionValue(int &i,
                        const std::string &opt,
                        std::string &out_value,
                        bool &out_option) {
        int next_idx = i + 1;
        if (next_idx >= argc) {
            _error = "value must be specified for option: " + opt;
            return false;
        }
        
        out_value = argv[next_idx];
        out_option = true;
        
        // Make the parser skip the next arg.
        i = next_idx;
        
        return true;
    }
    
    int argc;
    const char **argv;
    
    std::string _input_path;
    bool _verbose = false;
    bool _has_dest_path = false;
    std::string _dest_path;
    bool _has_library_path = false;
    std::string _library_path;
};

struct Extractor : public ErrorReporting {
    Extractor(const std::string &input,
              const std::string &library,
              const std::string &dest)
        : _input_path(input), _dest_path(dest)
    {
        printVerbose("loading the extractor library...");
        auto lib_handle = dlopen(library.c_str(), RTLD_NOW);
        if (!lib_handle) {
            _error = "failed to load the extractor library: " + library;
            return;
        }
        
        auto extract_func = dlsym(lib_handle, "dyld_shared_cache_extract_dylibs_progress");
        if (!extract_func) {
            _error = "failed to resolve the extract function";
            return;
        }
        
        _extract_func = reinterpret_cast<ExtractFunc>(extract_func);
        
        _has_error = false;
        printVerbose("library loaded!");
    }
    
    bool extract() {
        printVerbose("verifying the cache file, this may take a minute");
        
        auto sema = dispatch_semaphore_create(1);
        
        __block bool first_progress_report = true;
        int result = _extract_func(_input_path.c_str(), _dest_path.c_str(),
                      ^(unsigned int current, unsigned int total) {
            dispatch_semaphore_wait(sema, DISPATCH_TIME_FOREVER);
            
            if (first_progress_report) {
                printVerbose("the cache file seems to be good!");
                first_progress_report = false;
            }
            std::cout << "extracting files... (" << current << "/" << total << ")\r";
            std::cout.flush();
            
            dispatch_semaphore_signal(sema);
        });
        
        std::cout << std::endl;
        
        return result == 0;
    }
    
private:
    using ProgressBlock = void (^)(unsigned current, unsigned total);
    using ExtractFunc = int (*)(const char* shared_cache_file_path,
                                const char* extraction_root_path,
                                ProgressBlock progress);
    
    ExtractFunc _extract_func;
    
    std::string _input_path;
    std::string _dest_path;
};

int main(int argc, const char *argv[]) {
    g_cmdline = argv[0];
    
    // Parse arguments.
    Options opts(argc, argv);
    if (opts.hasError()) {
        printUsage();
        printError(opts.error());
        return 1;
    }
    
    g_verbose = opts.verbose();
    
    // Resolve the destination path (user specified or current working directory).
    std::string dest_path = ([&]() {
        if (opts.hasDestPath()) {
            return opts.destPath();
        }
        
        char *cwd = getcwd(nullptr, 0);
        if (!cwd) {
            printError("failed to get current working directory");
            abort();
        }
        std::string cwd_str = cwd;
        free(cwd);
        
        return cwd_str;
    })();
    printVerbose("extracted files will be in " + dest_path);
    
    // Prepare and start the actual extraction.
    Extractor extractor(opts.inputPath(),
                        opts.hasLibraryPath() ? opts.libraryPath() : default_extractor_path,
                        dest_path);
    if (extractor.hasError()) {
        printError(extractor.error());
        return 1;
    }
    if (extractor.extract()) {
        std::cout << "done, have fun!" << std::endl;
    } else {
        std::cout << "extract failed!" << std::endl;
        return 1;
    }
    
    return 0;
}
