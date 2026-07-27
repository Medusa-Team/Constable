CC ?= cc
CFLAGS ?= -Wall -Wextra -Wpedantic

TESTS = decision_test dynamic_argv_test alu_mixed_compare_test string_utils_test \
	mcp_validate_test object_string_test version_buffer_test

.PHONY: all clean

all: $(TESTS) ../constable
	./decision_test
	./dynamic_argv_test
	./alu_mixed_compare_test
	./string_utils_test
	./mcp_validate_test
	./object_string_test
	./version_buffer_test
	sh ./policy_compile_test.sh ../constable fixtures

../constable:
	$(MAKE) -C .. constable

decision_test: decision_test.c ../decision.c ../decision.h
	$(CC) $(CFLAGS) -std=c11 -I.. -o $@ decision_test.c ../decision.c

dynamic_argv_test: dynamic_argv_test.c ../../libmcompiler/libmcompiler.a
	$(CC) $(CFLAGS) -std=c11 -I../../libmcompiler -o $@ \
		dynamic_argv_test.c ../../libmcompiler/libmcompiler.a

alu_mixed_compare_test: alu_mixed_compare_test.c ../language/alu.c \
	../string_utils.c
	$(CC) $(CFLAGS) -std=c11 -I.. -o $@ \
		alu_mixed_compare_test.c ../language/alu.c ../string_utils.c

string_utils_test: string_utils_test.c ../string_utils.c ../string_utils.h
	$(CC) $(CFLAGS) -std=c11 -I.. -o $@ \
		string_utils_test.c ../string_utils.c

mcp_validate_test: mcp_validate_test.c ../mcp/validate.c ../mcp/validate.h
	$(CC) $(CFLAGS) -std=c11 -I.. -o $@ \
		mcp_validate_test.c ../mcp/validate.c

object_string_test: object_string_test.c ../object_endian.c ../object.h
	$(CC) $(CFLAGS) -std=c11 -I.. -o $@ \
		object_string_test.c ../object_endian.c

version_buffer_test: version_buffer_test.c ../../libmcompiler/libmcompiler.a
	$(CC) $(CFLAGS) -std=c11 -I../../libmcompiler -o $@ \
		version_buffer_test.c ../../libmcompiler/libmcompiler.a

clean:
	rm -rf $(TESTS) $(addsuffix .dSYM,$(TESTS))
