git clone https://github.com/google/googletest.git     || exit
cd googletest                                          || exit
mkdir build                                            || exit
cd build                                               || exit
cmake --build-64bit -DCMAKE_BUILD_TYPE=Release CXXFLAGS="-Ofast -mavx -m64 -march=native -flto -g0 -DNDEBUG" .. || exit
make -j                                                || exit
cd ../../                                              || exit