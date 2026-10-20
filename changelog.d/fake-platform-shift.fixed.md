**A shift by 40 of a `size_t` in the fake platform's memory budget**
(`tests/fake/fake_storage.cpp`) was not a valid shift on a 32-bit target; it is
now half the address space.
