/*
 * Copyright (c) 2016 Cisco and/or its affiliates.
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
/**
 * @brief
 * A Data-Path Object is an object that represents actions that are
 * applied to packets are they are switched through VPP.
 * 
 * The DPO is a base class that is specialised by other objects to provide
 * concreate actions
 *
 * The VLIB graph nodes are graph of types, the DPO graph is a graph of instances.
 */

#include <vnet/dpo/dpo.h>
#include <vnet/ip/lookup.h>
#include <vnet/ip/format.h>
#include <vnet/adj/adj.h>

#include <vnet/dpo/load_balance.h>
#include <vnet/dpo/mpls_label_dpo.h>
#include <vnet/dpo/lookup_dpo.h>
#include <vnet/dpo/drop_dpo.h>
#include <vnet/dpo/receive_dpo.h>
#include <vnet/dpo/punt_dpo.h>
#include <vnet/dpo/classify_dpo.h>

/**
 * Array of char* names for the DPO types and protos
 */
static const char* dpo_type_names[] = DPO_TYPES;
static const char* dpo_proto_names[] = DPO_PROTOS;

/**
 * @brief Vector of virtual function tables for the DPO types
 *
 * This is a vector so we can dynamically register new DPO types in plugins.
 */
static dpo_vft_t *dpo_vfts;

/**
 * @brief vector of graph node names associated with each DPO type and protocol.
 *
 *   dpo_nodes[child_type][child_proto][node_X] = node_name;
 * i.e.
 *   dpo_node[DPO_LOAD_BALANCE][DPO_PROTO_IP4][0] = "ip4-lookup"
 *   dpo_node[DPO_LOAD_BALANCE][DPO_PROTO_IP4][1] = "ip4-load-balance"
 *
 * This is a vector so we can dynamically register new DPO types in plugins.
 */
static const char* const * const ** dpo_nodes;

/**
 * @brief Vector of edge indicies from parent DPO nodes to child
 *
 * dpo_edges[child_type][child_proto][parent_type] = edge_index
 *
 * This array is derived at init time from the dpo_nodes above. Note that
 * the third dimension in dpo_nodes is lost, hence, the edge index from each
 * node MUST be the same.
 *
 * Note that this array is child type specific, not child instance specific.
 */
static u32 ***dpo_edges;

/**
 * @brief The DPO type value that can be assigend to the next dynamic
 *        type registration.
 */
static dpo_type_t dpo_dynamic = DPO_LAST;

u8 *
format_dpo_type (u8 * s, va_list * args)
{
    dpo_type_t type = va_arg (*args, int);

    s = format(s, "%s", dpo_type_names[type]);

    return (s);
}

u8 *
format_dpo_id (u8 * s, va_list * args)
{
    dpo_id_t *dpo = va_arg (*args, dpo_id_t*);
    u32 indent = va_arg (*args, u32);

    s = format(s, "[@%d]: ", dpo->dpoi_next_node);

    if (NULL != dpo_vfts[dpo->dpoi_type].dv_format)
    {
        return (format(s, "%U",
                       dpo_vfts[dpo->dpoi_type].dv_format,
                       dpo->dpoi_index,
                       indent));
    }

    switch (dpo->dpoi_type)
    {
    case DPO_FIRST:
	s = format(s, "unset");
	break;
    default:
	s = format(s, "unknown");
	break;
    }
    return (s);
}

u8 *
format_dpo_proto (u8 * s, va_list * args)
{
    dpo_proto_t proto = va_arg (*args, int);

    return (format(s, "%s", dpo_proto_names[proto]));
}

void
dpo_set (dpo_id_t *dpo,
	 dpo_type_t type,
	 dpo_proto_t proto,
	 index_t index)
{
    dpo_id_t tmp = *dpo;

    dpo->dpoi_type = type;
    dpo->dpoi_proto = proto,
    dpo->dpoi_index = index;

    if (DPO_ADJACENCY == type)
    {
	/*
	 * set the adj subtype
	 */
	ip_adjacency_t *adj;

	adj = adj_get(index);

	switch (adj->lookup_next_index)
	{
	case IP_LOOKUP_NEXT_ARP:
	    dpo->dpoi_type = DPO_ADJACENCY_INCOMPLETE;
	    break;
	case IP_LOOKUP_NEXT_MIDCHAIN:
	    dpo->dpoi_type = DPO_ADJACENCY_MIDCHAIN;
	    break;
	default:
	    break;
	}
    }
    dpo_lock(dpo);
    dpo_unlock(&tmp);
}

void
dpo_reset (dpo_id_t *dpo)
{
    dpo_set(dpo, DPO_FIRST, DPO_PROTO_NONE, INDEX_INVALID);
}

/**
 * \brief
 * Compare two Data-path objects
 *
 * like memcmp, return 0 is matching, !0 otherwise.
 */
int
dpo_cmp (const dpo_id_t *dpo1,
	 const dpo_id_t *dpo2)
{
    int res;

    res = dpo1->dpoi_type - dpo2->dpoi_type;

    if (0 != res) return (res);

    return (dpo1->dpoi_index - dpo2->dpoi_index);
}

void
dpo_copy (dpo_id_t *dst,
	  const dpo_id_t *src)
{
    dpo_id_t tmp = *dst;

    /*
     * the destination is written in a single u64 write - hence atomically w.r.t
     * any packets inflight.
     */
    *((u64*)dst) = *(u64*)src; 

    dpo_lock(dst);
    dpo_unlock(&tmp);    
}

int
dpo_is_adj (const dpo_id_t *dpo)
{
    return ((dpo->dpoi_type == DPO_ADJACENCY) ||
	    (dpo->dpoi_type == DPO_ADJACENCY_INCOMPLETE) ||
	    (dpo->dpoi_type == DPO_ADJACENCY_MIDCHAIN) ||
	    (dpo->dpoi_type == DPO_ADJACENCY_GLEAN));
}

void
dpo_register (dpo_type_t type,
	      const dpo_vft_t *vft,
              const char * const * const * nodes)
{
    vec_validate(dpo_vfts, type);
    dpo_vfts[type] = *vft;

    vec_validate(dpo_nodes, type);
    dpo_nodes[type] = nodes;
}

dpo_type_t
dpo_register_new_type (const dpo_vft_t *vft,
                       const char * const * const * nodes)
{
    dpo_type_t type = dpo_dynamic++;

    dpo_register(type, vft, nodes);

    return (type);
}

void
dpo_lock (dpo_id_t *dpo)
{
    if (!dpo_id_is_valid(dpo))
	return;

    dpo_vfts[dpo->dpoi_type].dv_lock(dpo);
}

void
dpo_unlock (dpo_id_t *dpo)
{
    if (!dpo_id_is_valid(dpo))
	return;

    dpo_vfts[dpo->dpoi_type].dv_unlock(dpo);
}


static u32
dpo_get_next_node (dpo_type_t child_type,
                   dpo_proto_t child_proto,
                   const dpo_id_t *parent_dpo)
{
    dpo_proto_t parent_proto;
    dpo_type_t parent_type;

    parent_type = parent_dpo->dpoi_type;
    parent_proto = parent_dpo->dpoi_proto;

    vec_validate(dpo_edges, child_type);
    vec_validate(dpo_edges[child_type], child_proto);
    vec_validate_init_empty(dpo_edges[child_type][child_proto],
                            parent_dpo->dpoi_type, ~0);

    /*
     * if the edge index has not yet been created for this node to node transistion
     */
    if (~0 == dpo_edges[child_type][child_proto][parent_type])
    {
        vlib_node_t *parent_node, *child_node;
        vlib_main_t *vm;
        u32 edge ,pp, cc;

        vm = vlib_get_main();

        ASSERT(NULL != dpo_nodes[child_type]);
        ASSERT(NULL != dpo_nodes[child_type][child_proto]);
        ASSERT(NULL != dpo_nodes[parent_type]);
        ASSERT(NULL != dpo_nodes[parent_type][parent_proto]);

        pp = 0;

        /*
         * create a graph arc from each of the parent's registered node types,
         * to each of the childs.
         */
        while (NULL != dpo_nodes[child_type][child_proto][pp])
        {
            parent_node =
                vlib_get_node_by_name(vm,
                                      (u8*) dpo_nodes[child_type][child_proto][pp]);

            cc = 0;

            while (NULL != dpo_nodes[parent_type][child_proto][cc])
            {
                child_node =
                    vlib_get_node_by_name(vm,
                                          (u8*) dpo_nodes[parent_type][parent_proto][cc]);

                edge = vlib_node_add_next(vm,
                                          parent_node->index,
                                          child_node->index);

                if (~0 == dpo_edges[child_type][child_proto][parent_type])
                {
                    dpo_edges[child_type][child_proto][parent_type] = edge;
                }
                else
                {
                    ASSERT(dpo_edges[child_type][child_proto][parent_type] == edge);
                }
                cc++;
            }
            pp++;
        }
    }

    return (dpo_edges[child_type][child_proto][parent_type]);
}

/**
 * @brief Stack one DPO object on another, and thus establish a child parent
 * relationship. The VLIB graph arc used is taken from the parent and child types
 * passed.
 */
static void
dpo_stack_i (u32 edge,
             dpo_id_t *dpo,
             const dpo_id_t *parent)
{
    /*
     * in order to get an atomic update of the parent we create a temporary,
     * from a copy of the child, and add the next_node. then we copy to the parent
     */
    dpo_id_t tmp = DPO_NULL;
    dpo_copy(&tmp, parent);

    /*
     * get the edge index for the parent to child VLIB graph transisition
     */
    tmp.dpoi_next_node = edge;

    /*
     * this update is atomic.
     */
    dpo_copy(dpo, &tmp);

    dpo_reset(&tmp);
}

/**
 * @brief Stack one DPO object on another, and thus establish a child-parent
 * relationship. The VLIB graph arc used is taken from the parent and child types
 * passed.
 */
void
dpo_stack (dpo_type_t child_type,
           dpo_proto_t child_proto,
           dpo_id_t *dpo,
           const dpo_id_t *parent)
{
    dpo_stack_i(dpo_get_next_node(child_type, child_proto, parent), dpo, parent);
}

/**
 * @brief Stack one DPO object on another, and thus establish a child parent
 * relationship. A new VLIB graph arc is created from the child node passed
 * to the nodes registered by the parent. The VLIB infra will ensure this arc
 * is added only once.
 */
void
dpo_stack_from_node (u32 child_node_index,
                     dpo_id_t *dpo,
                     const dpo_id_t *parent)
{
    dpo_proto_t parent_proto;
    vlib_node_t *parent_node;
    dpo_type_t parent_type;
    vlib_main_t *vm;
    u32 edge;

    parent_type = parent->dpoi_type;
    parent_proto = parent->dpoi_proto;

    vm = vlib_get_main();

    ASSERT(NULL != dpo_nodes[parent_type]);
    ASSERT(NULL != dpo_nodes[parent_type][parent_proto]);

    parent_node =
        vlib_get_node_by_name(vm, (u8*) dpo_nodes[parent_type][parent_proto][0]);

    edge = vlib_node_add_next(vm,
                              child_node_index,
                              parent_node->index);

    dpo_stack_i(edge, dpo, parent);
}

static clib_error_t *
dpo_module_init (vlib_main_t * vm)
{
    drop_dpo_module_init();
    punt_dpo_module_init();
    receive_dpo_module_init();
    load_balance_module_init();
    mpls_label_dpo_module_init();
    classify_dpo_module_init();
    lookup_dpo_module_init();

    return (NULL);
}

VLIB_INIT_FUNCTION(dpo_module_init);
