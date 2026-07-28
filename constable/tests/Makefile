CC ?= cc
CFLAGS ?= -Wall -Wextra -Wpedantic

TESTS = decision_test dynamic_argv_test alu_mixed_compare_test string_utils_test \
	mcp_validate_test object_string_test version_buffer_test mlibc_format_test
TESTS += dynamic_buffer_test comm_buffer_test register_bounds_test
TESTS += checked_math_test lexer_growth_test
TESTS += linked_list_test tree_path_test
TESTS += rbac_roles_test
TESTS += rbac_rotate_test
TESTS += vs_test
TESTS += cli_options_test
TESTS += worker_pool_test
TESTS += fallback_policy_test
TESTS += domain_rule_test
TESTS += approval_test
TESTS += config_approval_test

.PHONY: all clean

all: $(TESTS) ../constable
	./decision_test
	./dynamic_argv_test
	./alu_mixed_compare_test
	./string_utils_test
	./mcp_validate_test
	./object_string_test
	./version_buffer_test
	./mlibc_format_test
	./dynamic_buffer_test
	./comm_buffer_test
	./register_bounds_test
	./checked_math_test
	./lexer_growth_test
	./linked_list_test
	./tree_path_test
	./rbac_roles_test
	./rbac_rotate_test
	./vs_test
	./cli_options_test
	./worker_pool_test
	./fallback_policy_test
	./domain_rule_test
	./approval_test
	./config_approval_test fixtures/approval.conf
	sh ./cli_integration_test.sh ../constable
	sh ./retired_components_test.sh
	sh ./policy_compile_test.sh ../constable fixtures
	sh ./historical_policy_test.sh ../constable
	sh ./policy_runtime_test.sh ../constable fixtures
	sh ./policy_event_runtime_test.sh ../constable fixtures
	sh ./policy_inspect_test.sh ../constable fixtures
	sh ./policy_validate_test.sh ../constable fixtures

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
	$(CC) $(CFLAGS) -std=c11 -I.. -I../../include -o $@ \
		mcp_validate_test.c ../mcp/validate.c

object_string_test: object_string_test.c ../object_endian.c ../object.h
	$(CC) $(CFLAGS) -std=c11 -I.. -o $@ \
		object_string_test.c ../object_endian.c

version_buffer_test: version_buffer_test.c ../../libmcompiler/libmcompiler.a
	$(CC) $(CFLAGS) -std=c11 -I../../libmcompiler -o $@ \
		version_buffer_test.c ../../libmcompiler/libmcompiler.a

mlibc_format_test: mlibc_format_test.c ../Mlibc/mlibc_format.c \
	../Mlibc/mlibc_format.h
	$(CC) $(CFLAGS) -std=c11 -I.. -o $@ \
		mlibc_format_test.c ../Mlibc/mlibc_format.c

dynamic_buffer_test: dynamic_buffer_test.c ../../libmcompiler/libmcompiler.a
	$(CC) $(CFLAGS) -std=c11 -I../../libmcompiler -o $@ \
		dynamic_buffer_test.c ../../libmcompiler/libmcompiler.a

comm_buffer_test: comm_buffer_test.c ../comm_buf.c ../language/execute_stack.c
	$(CC) $(CFLAGS) -std=c11 -I.. -o $@ comm_buffer_test.c \
		../comm_buf.c ../language/execute_stack.c -pthread

register_bounds_test: register_bounds_test.c ../language/alu2.c \
		../object_endian.c
	$(CC) $(CFLAGS) -std=c11 -I.. -o $@ register_bounds_test.c \
		../language/alu2.c ../object_endian.c

checked_math_test: checked_math_test.c
	$(CC) $(CFLAGS) -std=c11 -I../../libmcompiler -o $@ $<

lexer_growth_test: lexer_growth_test.c ../../libmcompiler/libmcompiler.a
	$(CC) $(CFLAGS) -std=c11 -I../../libmcompiler -o $@ $< \
		../../libmcompiler/libmcompiler.a

linked_list_test: linked_list_test.c ../../libmcompiler/libmcompiler.a
	$(CC) $(CFLAGS) -std=c11 -I../../libmcompiler -o $@ $< \
		../../libmcompiler/libmcompiler.a

tree_path_test: tree_path_test.c ../tree.c
	$(CC) $(CFLAGS) -std=c11 -I.. -I../../libmcompiler -o $@ \
		tree_path_test.c ../tree.c

rbac_roles_test: rbac_roles_test.c ../rbac/roles.c ../string_utils.c
	$(CC) $(CFLAGS) -std=c11 -I.. -o $@ rbac_roles_test.c \
		../rbac/roles.c ../string_utils.c -pthread

rbac_rotate_test: rbac_rotate_test.c ../rbac/rotate.c
	$(CC) $(CFLAGS) -std=c11 -I.. -I../../libmcompiler -o $@ \
		rbac_rotate_test.c ../rbac/rotate.c

vs_test: vs_test.c ../vs.c ../vs.h
	$(CC) $(CFLAGS) -std=c11 -I.. -o $@ vs_test.c ../vs.c -pthread

cli_options_test: cli_options_test.c ../cli_options.c ../cli_options.h
	$(CC) $(CFLAGS) -std=c11 -I.. -o $@ cli_options_test.c \
		../cli_options.c

worker_pool_test: worker_pool_test.c ../threading.c ../threading.h
	$(CC) $(CFLAGS) -std=c11 -I.. -o $@ worker_pool_test.c \
		../threading.c

fallback_policy_test: fallback_policy_test.c ../fallback_policy.c \
		../fallback_policy.h
	$(CC) $(CFLAGS) -std=c11 -I.. -I../../include -o $@ fallback_policy_test.c \
		../fallback_policy.c

domain_rule_test: domain_rule_test.c ../domain_rule.c ../domain_rule.h
	$(CC) $(CFLAGS) -std=c11 -I.. -I../../include -o $@ \
		domain_rule_test.c ../domain_rule.c

approval_test: approval_test.c ../approval.c ../approval.h
	$(CC) $(CFLAGS) -std=c11 -I.. -I../../include -o $@ approval_test.c \
		../approval.c

config_approval_test: config_approval_test.c ../mcp/conf_lang.c \
		../approval.c ../string_utils.c ../../libmcompiler/libmcompiler.a
	$(CC) $(CFLAGS) -std=c11 -I.. -I../../libmcompiler -I../../include \
		-o $@ config_approval_test.c ../mcp/conf_lang.c ../approval.c \
		../string_utils.c ../../libmcompiler/libmcompiler.a -pthread

clean:
	rm -rf $(TESTS) $(addsuffix .dSYM,$(TESTS))
