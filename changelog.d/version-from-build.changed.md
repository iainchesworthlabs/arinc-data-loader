The three applications take the version they report (`QCoreApplication::applicationVersion`) from the
project version CMake stamps into the build, instead of a `0.1.0` written in each `main.cpp`, so a
release cannot ship a program that says one version and a package that says another.
