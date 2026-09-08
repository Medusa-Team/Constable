// SPDX-License-Identifier: GPL-2.0

#include "../rbac/rbac.h"
#include "../generic.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

pthread_key_t errstr_key;
char *Out_of_memory = "Out of memory";
char *Name_too_long = "Name too long";

struct class_s *rbac_user_class;
struct class_s *rbac_perm_class;
struct class_s *rbac_role_class;
struct class_s *rbac_ROLE_class;
struct tree_type_s *rbac_t_user;
struct tree_type_s *rbac_t_perm;
struct tree_type_s *rbac_t_role;
struct tree_type_s *rbac_t_ROLE;
struct comm_s *rbac_comm;

static int checks;
static int failures;

#define EXPECT_TRUE(condition, description)				\
	do {								\
		checks++;						\
		if (!(condition)) {					\
			fprintf(stderr, "FAIL: %s\n", description);	\
			failures++;					\
		}							\
	} while (0)

void vs_clear(vs_t *set)
{
	memset(set, 0, VS_WORDS * sizeof(*set));
}

void vs_add(const vs_t *from, vs_t *to)
{
	size_t index;

	for (index = 0; index < VS_WORDS; index++)
		to[index] |= from[index];
}

int generic_set_handler(struct class_handler_s *handler,
			struct comm_s *comm, struct object_s *object)
{
	(void)handler;
	(void)comm;
	(void)object;
	return 0;
}

vs_t *space_get_vs(struct space_s *space)
{
	(void)space;
	return NULL;
}

static void test_hierarchy_invariants(struct role_s *parent,
				      struct role_s *child)
{
	EXPECT_TRUE(rbac_set_hierarchy(NULL, child) == -1,
		    "a hierarchy requires a parent");
	EXPECT_TRUE(rbac_set_hierarchy(parent, NULL) == -1,
		    "a hierarchy requires a child");
	EXPECT_TRUE(rbac_set_hierarchy(parent, parent) == -1,
		    "a role cannot inherit from itself");
	EXPECT_TRUE(parent->sub == NULL && parent->sup == NULL,
		    "self-inheritance leaves the role unchanged");
	EXPECT_TRUE(rbac_set_hierarchy(parent, child) == 0,
		    "a valid parent-child relationship is accepted");
	EXPECT_TRUE(parent->sub && parent->sub->sub_role == child &&
		    child->sup == parent->sub,
		    "the hierarchy is linked in both directions");
	EXPECT_TRUE(rbac_set_hierarchy(parent, child) == -1,
		    "a duplicate hierarchy edge is rejected");
	EXPECT_TRUE(parent->sub && parent->sub->next_sub == NULL &&
		    child->sup && child->sup->next_sup == NULL,
		    "duplicate rejection leaves exactly one edge");
	EXPECT_TRUE(rbac_set_hierarchy(child, parent) == -1,
		    "an indirect hierarchy cycle is rejected");
	EXPECT_TRUE(rbac_del_hierarchy(parent, child) == 0,
		    "an existing hierarchy edge can be removed");
	EXPECT_TRUE(parent->sub == NULL && child->sup == NULL,
		    "hierarchy removal unlinks both directions");
	EXPECT_TRUE(rbac_del_hierarchy(parent, child) == -1,
		    "removing a missing edge is rejected");
}

static void test_user_assignment_bounds(struct role_s *parent,
					struct role_s *child)
{
	struct user_s user = { 0 };
	int index;

	user.nr_roles = USER_MAX_ROLES;
	for (index = 0; index < USER_MAX_ROLES; index++)
		user.roles[index] = parent;
	EXPECT_TRUE(rbac_add_ua(&user, child) == -1,
		    "a full role vector rejects another assignment");
	EXPECT_TRUE(user.ua == NULL && child->ua == NULL,
		    "full-vector rejection creates no linked assignment");

	user.roles[7] = NULL;
	EXPECT_TRUE(rbac_add_ua(&user, child) == 0,
		    "a vacant role-vector slot can be reused");
	EXPECT_TRUE(user.roles[7] == child && user.nr_roles == USER_MAX_ROLES,
		    "slot reuse preserves the bounded high-water mark");
	EXPECT_TRUE(user.ua && user.ua == child->ua,
		    "a user assignment is linked from user and role");
	EXPECT_TRUE(rbac_add_ua(&user, child) == -1,
		    "a duplicate user assignment is rejected");
	EXPECT_TRUE(user.ua && user.ua->next_role == NULL &&
		    child->ua && child->ua->next_user == NULL,
		    "duplicate rejection leaves exactly one assignment");
	EXPECT_TRUE(rbac_del_ua(&user, child) == 0,
		    "an existing user assignment can be removed");
	EXPECT_TRUE(user.roles[7] == NULL && user.ua == NULL &&
		    child->ua == NULL,
		    "assignment removal clears every link");

	user.nr_roles = USER_MAX_ROLES + 1;
	EXPECT_TRUE(rbac_add_ua(&user, child) == -1,
		    "a corrupt role count is rejected before indexing");
}

static void test_failed_delete_unlocks(void)
{
	struct role_s unknown = { 0 };
	int lock_result;

	EXPECT_TRUE(rbac_role_del(NULL) == -1,
		    "role deletion rejects a null role");
	EXPECT_TRUE(rbac_role_del(&unknown) == -1,
		    "role deletion rejects an unregistered role");
	lock_result = pthread_rwlock_trywrlock(&rbac_roles_lock);
	EXPECT_TRUE(lock_result == 0,
		    "failed deletion releases the global role write lock");
	if (lock_result == 0)
		pthread_rwlock_unlock(&rbac_roles_lock);
}

int main(void)
{
	char *thread_error = NULL;
	struct role_s *parent;
	struct role_s *child;

	if (pthread_key_create(&errstr_key, NULL) != 0 ||
	    pthread_setspecific(errstr_key, &thread_error) != 0)
		return EXIT_FAILURE;

	parent = rbac_role_add("parent");
	child = rbac_role_add("child");
	EXPECT_TRUE(parent != NULL && child != NULL,
		    "test roles are created");
	if (parent && child) {
		test_hierarchy_invariants(parent, child);
		test_user_assignment_bounds(parent, child);
	}
	if (child)
		EXPECT_TRUE(rbac_role_del(child) == 0,
			    "the child role is deleted");
	if (parent)
		EXPECT_TRUE(rbac_role_del(parent) == 0,
			    "the parent role is deleted");
	EXPECT_TRUE(rbac_roles == NULL, "the role registry is empty");
	test_failed_delete_unlocks();

	pthread_key_delete(errstr_key);
	if (failures) {
		fprintf(stderr, "RBAC roles: %d failure(s)\n", failures);
		return EXIT_FAILURE;
	}
	printf("RBAC roles: %d checks passed\n", checks);
	return EXIT_SUCCESS;
}
