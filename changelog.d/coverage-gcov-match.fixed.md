The `coverage` target passes the `gcov` that matches the compiler, and the coverage gate
fails when it measured too little to mean anything (gcovr reports 0 of 0 lines as 100%).
