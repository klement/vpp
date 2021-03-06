/*
 * Copyright (c) 2015 Cisco and/or its affiliates.
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at:
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#ifndef __included_ip6_hop_by_hop_ioam_h__
#define __included_ip6_hop_by_hop_ioam_h__

#include <vnet/ip/ip6_hop_by_hop.h>
#include <vnet/ip/ip6_hop_by_hop_packet.h>
#include <vnet/ip/ip.h>

typedef struct {
  /* The current rewrite we're using */
  u8 * rewrite;

  /* Trace data processing callback */
  void *ioam_end_of_path_cb;
  /* Configuration data */
  /* Adjacency */
  ip6_address_t adj;
#define IOAM_HBYH_ADD  0
#define IOAM_HBYH_MOD  1
#define IOAM_HBYH_POP  2
  u8 ioam_flag;
  /* time scale transform. Joy. */
  u32 unix_time_0;
  f64 vlib_time_0;


  /* Trace option */
  u8 has_trace_option;

  /* Pot option */
  u8 has_pot_option;

#define PPC_NONE  0
#define PPC_ENCAP 1
#define PPC_DECAP 2
  u8 has_ppc_option;

#define TSP_SECONDS              0
#define TSP_MILLISECONDS         1
#define TSP_MICROSECONDS         2
#define TSP_NANOSECONDS          3
  
  /* Array of function pointers to ADD and POP HBH option handling routines */
  u8 options_size[256];
  int (*add_options[256])(u8 *rewrite_string, u8 *rewrite_size);
  int (*pop_options[256])(ip6_header_t *ip, ip6_hop_by_hop_option_t *opt);
  int (*get_sizeof_options[256])(u32 *rewrite_size);
  
  /* convenience */
  vlib_main_t * vlib_main;
  vnet_main_t * vnet_main;
} ip6_hop_by_hop_ioam_main_t;

extern ip6_hop_by_hop_ioam_main_t ip6_hop_by_hop_ioam_main;

extern u8 * format_path_map(u8 * s, va_list * args);
extern clib_error_t *
ip6_ioam_enable(int has_trace_option, int has_pot_option,
                           int has_e2e_option);
extern int ip6_ioam_set_destination (ip6_address_t *addr, u32 mask_width,
                  u32 vrf_id, int is_add, int is_pop, int is_none);

extern clib_error_t * clear_ioam_rewrite_fn(void);

static inline u8 is_zero_ip4_address (ip4_address_t *a)
{
  return (a->as_u32 == 0);
}

static inline void copy_ip6_address (ip6_address_t *dst, ip6_address_t *src)
{
  dst->as_u64[0] = src->as_u64[0];
  dst->as_u64[1] = src->as_u64[1];
}

static inline void set_zero_ip6_address (ip6_address_t *a)
{
  a->as_u64[0] = 0;
  a->as_u64[1] = 0;
}

static inline u8 cmp_ip6_address (ip6_address_t *a1, ip6_address_t *a2)
{
  return ((a1->as_u64[0] == a2->as_u64[0]) && (a1->as_u64[1] == a2->as_u64[1]));
}
static inline u8 is_zero_ip6_address (ip6_address_t *a)
{
  return ((a->as_u64[0] == 0) && (a->as_u64[1] == 0));
}

int ip6_hbh_add_register_option (u8 option,
				 u8 size,
				 int rewrite_options(u8 *rewrite_string, u8 *size));
int ip6_hbh_add_unregister_option (u8 option);

int ip6_hbh_pop_register_option (u8 option,
				 int options(ip6_header_t *ip, ip6_hop_by_hop_option_t *opt));
int ip6_hbh_pop_unregister_option (u8 option);

int
ip6_hbh_get_sizeof_register_option (u8 option,
                            int get_sizeof_hdr_options(u32 *rewrite_size));

int
ip6_ioam_set_rewrite (u8 ** rwp, int has_trace_option,
		      int has_pot_option, int has_ppc_option);
#endif /* __included_ip6_hop_by_hop_ioam_h__ */
