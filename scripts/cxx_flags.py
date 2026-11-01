# C++-only compiler flags. Kept out of build_flags, which also reach the C
# compiler and make it warn about every one of them.

Import("env")

env.Append(CXXFLAGS=[
    "-frtti",
    "-fno-use-cxa-atexit",
])
