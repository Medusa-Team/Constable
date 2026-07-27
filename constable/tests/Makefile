CC ?= cc
CFLAGS ?= -Wall -Wextra -Wpedantic

TESTS = decision_test dynamic_argv_test alu_mixed_compare_test

.PHONY: all clean

all: $(TESTS) ../constable
	./decision_test
	./dynamic_argv_test
	./alu_mixed_compare_test
	sh ./policy_compile_test.sh ../constable fixtures

../constable:
	$(MAKE) -C .. constable

decision_test: decision_test.c ../decision.c ../decision.h
	$(CC) $(CFLAGS) -std=c11 -I.. -o $@ decision_test.c ../decision.c

dynamic_argv_test: dynamic_argv_test.c ../../libmcompiler/libmcompiler.a
	$(CC) $(CFLAGS) -std=c11 -I../../libmcompiler -o $@ \
		dynamic_argv_test.c ../../libmcompiler/libmcompiler.a

alu_mixed_compare_test: alu_mixed_compare_test.c ../language/alu.c
	$(CC) $(CFLAGS) -std=c11 -I.. -o $@ \
		alu_mixed_compare_test.c ../language/alu.c

clean:
	rm -rf $(TESTS) $(addsuffix .dSYM,$(TESTS))
