target("pvmp3dec")
    set_kind("static")

    add_files("src/*.cpp")
    add_includedirs("include", "src", {public=true})

    add_defines("OSCL_UNUSED_ARG(x)=(void)(x)")
    add_ldflags("-static", "-static-libstdc++","-static-libgcc")

target("mp3towav")
    set_kind("binary")
    
    add_files("test/*.cpp|mp3dec_test.cpp")
    add_includedirs("test")
    add_ldflags("-static", "-static-libstdc++","-static-libgcc")
    add_deps("pvmp3dec")