{
    "targets": [
        {
            "target_name": "emojs",
            "sources": [ "NodeEPOCDriver.cc" ],
            "include_dirs": [ "/usr/local/include" ],
            "cflags": [
              "-fPIC"
            ],
            "variables" : {
                "target_arch": "ia32",
                "host_arch":"ia32"
            },
            "conditions": [
                ['OS=="mac"', {
                    "cflags": [ "-m32" ],
                    "ldflags": [ "-m32" ],
                   "xcode_settings": {
                     "OTHER_CFLAGS": ["-ObjC++"],
                     "ARCHS": [ "i386" ]
                   },
                    "link_settings": {
                        "libraries": [
                         "/usr/local/lib/libedk.dylib",
                         "/usr/local/lib/libedk.1.dylib",
                         "/usr/local/lib/libedk_ultils_mac.dylib",
                         "/usr/local/lib/libiomp5.dylib"
                        ]
                    }
                }],
                ['OS=="linux"', {
                     "cflags": [ "-m32" ],
                     "ldflags": [ "-m32" ],
                    "xcode_settings": {
                      "ARCHS": [ "i386" ]
                    },
                     "link_settings": {
                         "libraries": [
                          "/usr/local/lib/libedk.so.1",
                          "/usr/local/lib/libhal.so.1",
                          "/usr/local/lib/libedk_utils.so",
                          "/usr/local/lib/libqwt.so.5"
                         ]
                     }
                 }]
             ]
        }
    ]
}
