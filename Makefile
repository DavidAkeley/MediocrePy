CC = clang -fPIC -mavx -D_POSIX_C_SOURCE=201112L -O2 -S -std=c11 -Wall -Wextra -Wc++-compat -I include -I src/inline -g
CC4 = clang -fPIC -mavx -D_POSIX_C_SOURCE=201112L -O3 -S -std=c11 -Wall -Wextra -Wc++-compat -I include -I src/inline -g
Cxx = clang -fPIC -mavx -D_POSIX_C_SOURCE=201112L -O2 -S -std=c++11 -Wall -Wextra -I include -I src/inline -g

LinkLib = clang -fPIC -lm -lpthread -shared
LinkTest = clang++ -lpthread

# bin/ is a bit of a misnomer since I'm really compiling to assembly instead of
# object files, so that I can see what the hell the compiler is actually up to.

bin/chunkutil.s: src/chunkutil.c src/inline/chunkutil.h include/convert.h
	$(CC4) src/chunkutil.c -o bin/chunkutil.s

bin/convert.s: src/convert.c include/convert.h
	$(CC4) src/convert.c -o bin/convert.s

bin/mean.s: src/mean.c include/mean.h include/convert.h src/inline/chunkutil.h src/inline/sigmautil.h
	$(CC4) src/mean.c -o bin/mean.s
	
bin/median.s: src/median.c include/median.h include/convert.h src/inline/sigmautil.h src/inline/chunkutil.h
	$(CC4) src/median.c -o bin/median.s

bin/testing.s: src/testing.cc
	$(Cxx) src/testing.cc -o bin/testing.s

bin/mediocre.so: bin/convert.s bin/mean.s
	$(LinkLib) bin/convert.s bin/mean.s
	
tests/bin/convert_test.s: tests/convert_test.c include/convert.h src/inline/testing.h
	$(CC) tests/convert_test.c -o tests/bin/convert_test.s

tests/bin/convert_test: bin/testing.s bin/convert.s tests/bin/convert_test.s
	$(LinkTest) bin/testing.s bin/convert.s tests/bin/convert_test.s -o tests/bin/convert_test

tests/bin/mean_test.s: tests/mean_test.c include/mean.h src/inline/testing.h
	$(CC) tests/mean_test.c -o tests/bin/mean_test.s
	
tests/bin/median_test.s: tests/median_test.c include/median.h src/inline/testing.h
	$(CC) tests/median_test.c -o tests/bin/median_test.s

tests/bin/mean_test: bin/testing.s bin/convert.s bin/chunkutil.s bin/mean.s tests/bin/mean_test.s
	$(LinkTest) bin/testing.s bin/convert.s bin/chunkutil.s bin/mean.s tests/bin/mean_test.s -o tests/bin/mean_test
	
tests/bin/median_test: bin/testing.s bin/convert.s bin/chunkutil.s bin/median.s tests/bin/median_test.s
	$(LinkTest) bin/testing.s bin/convert.s bin/chunkutil.s bin/median.s tests/bin/median_test.s -o tests/bin/median_test
	
	
	
