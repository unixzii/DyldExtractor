#  DyldExtractor

A command-line tool to extract dylib files from the **dyld shared cache** file.

Starting with macOS 11, standalone binaries of system libraries are not shipped with the system anymore. That means we cannot reverse-engineer those system libraries / frameworks directly. Fortunately, dyld provided a utility bundle for extracting image files from the cache file. This tool is just a wrapper of that utility bundle.

## Build

Clone the repo and build it with make:

```shell
git clone https://github.com/unixzii/DyldExtractor.git
cd DyldExtractor && make 
```

## Usage

```
OVERVIEW: dyld shared cache extractor

USAGE: ./dyld_extractor <cache file> [-v] [-d arg] [-l arg]

OPTIONS:
    -v print verbose messages
    -d set destination path for the extracted files
    -l set path to dsc_extractor library
```

## Notes for iOS

To extract iOS's dyld shared cache files, you may need to specify a different dsc_extractor library with `-l` option. The library can usually be found at `/path/to/Xcode.app/Contents/Developer/Platforms/iPhoneOS.platform/usr/lib/dsc_extractor.bundle`.
