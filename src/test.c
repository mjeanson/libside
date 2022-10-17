// SPDX-License-Identifier: MIT
/*
 * Copyright 2022 Mathieu Desnoyers <mathieu.desnoyers@efficios.com>
 */

#include <stdint.h>
#include <inttypes.h>
#include <stdlib.h>
#include <stdio.h>

#include <side/trace.h>
#include "tracer.h"

/* User code example */

static side_define_event(my_provider_event, "myprovider", "myevent", SIDE_LOGLEVEL_DEBUG,
	side_field_list(
		side_field(SIDE_TYPE_U32, "abc"),
		side_field(SIDE_TYPE_S64, "def"),
		side_field(SIDE_TYPE_DYNAMIC, "dynamic"),
	)
);

static
void test_fields(void)
{
	uint32_t uw = 42;
	int64_t sdw = -500;

	my_provider_event.enabled = 1;
	side_event(&my_provider_event, side_arg_list(side_arg_u32(uw), side_arg_s64(sdw),
		side_arg_dynamic(side_arg_dynamic_string("zzz"))));
}

static side_define_event(my_provider_event2, "myprovider", "myevent2", SIDE_LOGLEVEL_DEBUG,
	side_field_list(
		side_field_struct("structfield",
			side_field_list(
				side_field(SIDE_TYPE_U32, "x"),
				side_field(SIDE_TYPE_S64, "y"),
			)
		),
		side_field(SIDE_TYPE_U8, "z"),
	)
);

static
void test_struct(void)
{
	my_provider_event2.enabled = 1;
	side_event_cond(&my_provider_event2) {
		side_arg_define_vec(mystruct, side_arg_list(side_arg_u32(21), side_arg_s64(22)));
		side_event_call(&my_provider_event2, side_arg_list(side_arg_struct(&mystruct), side_arg_u8(55)));
	}
}

static side_define_event(my_provider_event_array, "myprovider", "myarray", SIDE_LOGLEVEL_DEBUG,
	side_field_list(
		side_field_array("arr", side_elem_type(SIDE_TYPE_U32), 3),
		side_field(SIDE_TYPE_S64, "v"),
	)
);

static
void test_array(void)
{
	my_provider_event_array.enabled = 1;
	side_event_cond(&my_provider_event_array) {
		side_arg_define_vec(myarray, side_arg_list(side_arg_u32(1), side_arg_u32(2), side_arg_u32(3)));
		side_event_call(&my_provider_event_array, side_arg_list(side_arg_array(&myarray), side_arg_s64(42)));
	}
}

static side_define_event(my_provider_event_vla, "myprovider", "myvla", SIDE_LOGLEVEL_DEBUG,
	side_field_list(
		side_field_vla("vla", side_elem_type(SIDE_TYPE_U32)),
		side_field(SIDE_TYPE_S64, "v"),
	)
);

static
void test_vla(void)
{
	my_provider_event_vla.enabled = 1;
	side_event_cond(&my_provider_event_vla) {
		side_arg_define_vec(myvla, side_arg_list(side_arg_u32(1), side_arg_u32(2), side_arg_u32(3)));
		side_event_call(&my_provider_event_vla, side_arg_list(side_arg_vla(&myvla), side_arg_s64(42)));
	}
}

/* 1D array visitor */

struct app_visitor_ctx {
	const uint32_t *ptr;
	uint32_t length;
};

static
enum side_visitor_status test_visitor(const struct side_tracer_visitor_ctx *tracer_ctx, void *_ctx)
{
	struct app_visitor_ctx *ctx = (struct app_visitor_ctx *) _ctx;
	uint32_t length = ctx->length, i;

	for (i = 0; i < length; i++) {
		const struct side_arg_vec elem = {
			.type = SIDE_TYPE_U32,
			.u = {
				.side_u32 = ctx->ptr[i],
			},
		};
		tracer_ctx->write_elem(tracer_ctx, &elem);
	}
	return SIDE_VISITOR_STATUS_OK;
}

static uint32_t testarray[] = { 1, 2, 3, 4, 5, 6, 7, 8 };

static side_define_event(my_provider_event_vla_visitor, "myprovider", "myvlavisit", SIDE_LOGLEVEL_DEBUG,
	side_field_list(
		side_field_vla_visitor("vlavisit", side_elem_type(SIDE_TYPE_U32), test_visitor),
		side_field(SIDE_TYPE_S64, "v"),
	)
);

static
void test_vla_visitor(void)
{
	my_provider_event_vla_visitor.enabled = 1;
	side_event_cond(&my_provider_event_vla_visitor) {
		struct app_visitor_ctx ctx = {
			.ptr = testarray,
			.length = SIDE_ARRAY_SIZE(testarray),
		};
		side_event_call(&my_provider_event_vla_visitor, side_arg_list(side_arg_vla_visitor(&ctx), side_arg_s64(42)));
	}
}

/* 2D array visitor */

struct app_visitor_2d_inner_ctx {
	const uint32_t *ptr;
	uint32_t length;
};

static
enum side_visitor_status test_inner_visitor(const struct side_tracer_visitor_ctx *tracer_ctx, void *_ctx)
{
	struct app_visitor_2d_inner_ctx *ctx = (struct app_visitor_2d_inner_ctx *) _ctx;
	uint32_t length = ctx->length, i;

	for (i = 0; i < length; i++) {
		const struct side_arg_vec elem = {
			.type = SIDE_TYPE_U32,
			.u = {
				.side_u32 = ctx->ptr[i],
			},
		};
		tracer_ctx->write_elem(tracer_ctx, &elem);
	}
	return SIDE_VISITOR_STATUS_OK;
}

struct app_visitor_2d_outer_ctx {
	const uint32_t (*ptr)[2];
	uint32_t length;
};

static
enum side_visitor_status test_outer_visitor(const struct side_tracer_visitor_ctx *tracer_ctx, void *_ctx)
{
	struct app_visitor_2d_outer_ctx *ctx = (struct app_visitor_2d_outer_ctx *) _ctx;
	uint32_t length = ctx->length, i;

	for (i = 0; i < length; i++) {
		struct app_visitor_2d_inner_ctx inner_ctx = {
			.ptr = ctx->ptr[i],
			.length = 2,
		};
		const struct side_arg_vec elem = side_arg_vla_visitor(&inner_ctx);
		tracer_ctx->write_elem(tracer_ctx, &elem);
	}
	return SIDE_VISITOR_STATUS_OK;
}

static uint32_t testarray2d[][2] = {
	{ 1, 2 },
	{ 33, 44 },
	{ 55, 66 },
};

static side_define_event(my_provider_event_vla_visitor2d, "myprovider", "myvlavisit2d", SIDE_LOGLEVEL_DEBUG,
	side_field_list(
		side_field_vla_visitor("vlavisit2d",
			side_elem(side_type_vla_visitor_decl(side_elem_type(SIDE_TYPE_U32), test_inner_visitor)), test_outer_visitor),
		side_field(SIDE_TYPE_S64, "v"),
	)
);

static
void test_vla_visitor_2d(void)
{
	my_provider_event_vla_visitor2d.enabled = 1;
	side_event_cond(&my_provider_event_vla_visitor2d) {
		struct app_visitor_2d_outer_ctx ctx = {
			.ptr = testarray2d,
			.length = SIDE_ARRAY_SIZE(testarray2d),
		};
		side_event_call(&my_provider_event_vla_visitor2d, side_arg_list(side_arg_vla_visitor(&ctx), side_arg_s64(42)));
	}
}

static int64_t array_fixint[] = { -444, 555, 123, 2897432587 };

static side_define_event(my_provider_event_array_fixint, "myprovider", "myarrayfixint", SIDE_LOGLEVEL_DEBUG,
	side_field_list(
		side_field_array("arrfixint", side_elem_type(SIDE_TYPE_S64), SIDE_ARRAY_SIZE(array_fixint)),
		side_field(SIDE_TYPE_S64, "v"),
	)
);

static
void test_array_fixint(void)
{
	my_provider_event_array_fixint.enabled = 1;
	side_event(&my_provider_event_array_fixint,
		side_arg_list(side_arg_array_s64(array_fixint), side_arg_s64(42)));
}

static int64_t vla_fixint[] = { -444, 555, 123, 2897432587 };

static side_define_event(my_provider_event_vla_fixint, "myprovider", "myvlafixint", SIDE_LOGLEVEL_DEBUG,
	side_field_list(
		side_field_vla("vlafixint", side_elem_type(SIDE_TYPE_S64)),
		side_field(SIDE_TYPE_S64, "v"),
	)
);

static
void test_vla_fixint(void)
{
	my_provider_event_vla_fixint.enabled = 1;
	side_event(&my_provider_event_vla_fixint,
		side_arg_list(side_arg_vla_s64(vla_fixint, SIDE_ARRAY_SIZE(vla_fixint)), side_arg_s64(42)));
}

static side_define_event(my_provider_event_dynamic_basic,
	"myprovider", "mydynamicbasic", SIDE_LOGLEVEL_DEBUG,
	side_field_list(
		side_field(SIDE_TYPE_DYNAMIC, "dynamic"),
	)
);

static
void test_dynamic_basic_type(void)
{
	my_provider_event_dynamic_basic.enabled = 1;
	side_event(&my_provider_event_dynamic_basic,
		side_arg_list(side_arg_dynamic(side_arg_dynamic_s16(-33))));
}

static side_define_event(my_provider_event_dynamic_vla,
	"myprovider", "mydynamicvla", SIDE_LOGLEVEL_DEBUG,
	side_field_list(
		side_field(SIDE_TYPE_DYNAMIC, "dynamic"),
	)
);

static
void test_dynamic_vla(void)
{
	side_arg_dynamic_define_vec(myvla, side_arg_list(side_arg_u32(1), side_arg_u32(2), side_arg_u32(3)));

	my_provider_event_dynamic_vla.enabled = 1;
	side_event(&my_provider_event_dynamic_vla,
		side_arg_list(side_arg_dynamic(side_arg_dynamic_vla(&myvla))));
}

static side_define_event(my_provider_event_dynamic_null,
	"myprovider", "mydynamicnull", SIDE_LOGLEVEL_DEBUG,
	side_field_list(
		side_field(SIDE_TYPE_DYNAMIC, "dynamic"),
	)
);

static
void test_dynamic_null(void)
{
	my_provider_event_dynamic_null.enabled = 1;
	side_event(&my_provider_event_dynamic_null,
		side_arg_list(side_arg_dynamic(side_arg_dynamic_null())));
}

static side_define_event(my_provider_event_dynamic_map,
	"myprovider", "mydynamicmap", SIDE_LOGLEVEL_DEBUG,
	side_field_list(
		side_field(SIDE_TYPE_DYNAMIC, "dynamic"),
	)
);

static
void test_dynamic_map(void)
{
	side_arg_dynamic_define_map(mymap,
		side_arg_list(
			side_arg_dynamic_field("a", side_arg_dynamic_u32(43)),
			side_arg_dynamic_field("b", side_arg_dynamic_string("zzz")),
			side_arg_dynamic_field("c", side_arg_dynamic_null())
		)
	);

	my_provider_event_dynamic_map.enabled = 1;
	side_event(&my_provider_event_dynamic_map,
		side_arg_list(side_arg_dynamic(side_arg_dynamic_map(&mymap))));
}

int main()
{
	test_fields();
	test_struct();
	test_array();
	test_vla();
	test_vla_visitor();
	test_vla_visitor_2d();
	test_array_fixint();
	test_vla_fixint();
	test_dynamic_basic_type();
	test_dynamic_vla();
	test_dynamic_null();
	test_dynamic_map();
	return 0;
}
