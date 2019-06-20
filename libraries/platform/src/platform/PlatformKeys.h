//
//  Created by Amer Cerkic 05/02/2019
//  Copyright 2019 High Fidelity, Inc.
//
//  Distributed under the Apache License, Version 2.0.
//  See the accompanying file LICENSE or http://www.apache.org/licenses/LICENSE-2.0.html
//
#ifndef hifi_platform_PlatformKeys_h
#define hifi_platform_PlatformKeys_h

namespace platform { namespace keys{
    // "UNKNOWN"
    extern const char*  UNKNOWN;

    namespace cpu {
        extern const char*  vendor;
        extern const char*  vendor_Intel;
        extern const char*  vendor_AMD;

        extern const char*  model;
        extern const char*  clockSpeed;
        extern const char*  numCores;
    }
    namespace gpu {
        extern const char*  vendor;
        extern const char*  vendor_NVIDIA;
        extern const char*  vendor_AMD;
        extern const char*  vendor_Intel;

        extern const char*  model;
        extern const char*  videoMemory;
        extern const char*  driver;
    }
    namespace display {
        extern const char*  description;
        extern const char*  name;
        extern const char*  coordsLeft;
        extern const char*  coordsRight;
        extern const char*  coordsTop;
        extern const char*  coordsBottom;
    }
    namespace memory {
        extern const char*  memTotal;
    }
    namespace computer {
        extern const char*  OS;
        extern const char*  OS_WINDOWS;
        extern const char*  OS_MACOS;
        extern const char*  OS_LINUX;
        extern const char*  OS_ANDROID;

        extern const char*  OSVersion;

        extern const char*  vendor;
        extern const char*  vendor_Apple;
        
        extern const char*  model;

        extern const char*  profileTier;
    }

    // Keys for categories used in json returned by getAll()
    extern const char*  CPUS;
    extern const char*  GPUS;
    extern const char*  DISPLAYS;
    extern const char*  MEMORY;
    extern const char*  COMPUTER;

} } // namespace plaform::keys

#endif
