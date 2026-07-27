CC ?= cc
CFLAGS ?= -Wall -Wextra -Wpedantic

TESTS = decision_test dynamic_argv_test

.PHONY: all clean

all: $(TESTS) ../constable
	./decision_test
	./dynamic_argv_test
	sh ./policy_compile_test.sh ../constable fixtures

../constable:
	$(MAKE) -C .. constable

decision_test: decision_test.c ../decision.c ../decision.h
	$(CC) $(CFLAGS) -std=c11 -I.. -o $@ decision_test.c ../decision.c

dynamic_argv_test: dynamic_argv_test.c ../../libmcompiler/libmcompiler.a
	$(CC) $(CFLAGS) -std=c11 -I../../libmcompiler -o $@ \
		dynamic_argv_test.c ../../libmcompiler/libmcompiler.a

clean:
	rm -rf $(TESTS) $(addsuffix .dSYM,$(TESTS))
