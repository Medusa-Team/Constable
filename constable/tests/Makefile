CC ?= cc
CFLAGS ?= -Wall -Wextra -Wpedantic

TESTS = decision_test

.PHONY: all clean

all: $(TESTS) ../constable
	./decision_test
	sh ./policy_compile_test.sh ../constable fixtures

../constable:
	$(MAKE) -C .. constable

decision_test: decision_test.c ../decision.c ../decision.h
	$(CC) $(CFLAGS) -std=c11 -I.. -o $@ decision_test.c ../decision.c

clean:
	rm -rf $(TESTS) $(addsuffix .dSYM,$(TESTS))
