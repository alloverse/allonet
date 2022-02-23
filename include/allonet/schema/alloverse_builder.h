#ifndef ALLOVERSE_BUILDER_H
#define ALLOVERSE_BUILDER_H

/* Generated by flatcc 0.6.1-dev FlatBuffers schema compiler for C by dvide.com */

#ifndef ALLOVERSE_READER_H
#include "alloverse_reader.h"
#endif
#ifndef FLATBUFFERS_COMMON_BUILDER_H
#include "flatbuffers_common_builder.h"
#endif
#include "flatcc/flatcc_prologue.h"
#ifndef flatbuffers_identifier
#define flatbuffers_identifier 0
#endif
#ifndef flatbuffers_extension
#define flatbuffers_extension "bin"
#endif

#define __Alloverse_Mat4_formal_args , const float v0[16]
#define __Alloverse_Mat4_call_args , v0
static inline Alloverse_Mat4_t *Alloverse_Mat4_assign(Alloverse_Mat4_t *p, const float v0[16])
{ flatbuffers_float_array_copy(p->m, v0, 16);
  return p; }
static inline Alloverse_Mat4_t *Alloverse_Mat4_copy(Alloverse_Mat4_t *p, const Alloverse_Mat4_t *p2)
{ flatbuffers_float_array_copy(p->m, p2->m, 16);
  return p; }
static inline Alloverse_Mat4_t *Alloverse_Mat4_assign_to_pe(Alloverse_Mat4_t *p, const float v0[16])
{ flatbuffers_float_array_copy_to_pe(p->m, v0, 16);
  return p; }
static inline Alloverse_Mat4_t *Alloverse_Mat4_copy_to_pe(Alloverse_Mat4_t *p, const Alloverse_Mat4_t *p2)
{ flatbuffers_float_array_copy_to_pe(p->m, p2->m, 16);
  return p; }
static inline Alloverse_Mat4_t *Alloverse_Mat4_assign_from_pe(Alloverse_Mat4_t *p, const float v0[16])
{ flatbuffers_float_array_copy_from_pe(p->m, v0, 16);
  return p; }
static inline Alloverse_Mat4_t *Alloverse_Mat4_copy_from_pe(Alloverse_Mat4_t *p, const Alloverse_Mat4_t *p2)
{ flatbuffers_float_array_copy_from_pe(p->m, p2->m, 16);
  return p; }
__flatbuffers_build_struct(flatbuffers_, Alloverse_Mat4, 64, 4, Alloverse_Mat4_file_identifier, Alloverse_Mat4_type_identifier)
__flatbuffers_define_fixed_array_primitives(flatbuffers_, Alloverse_Mat4, Alloverse_Mat4_t)

static const flatbuffers_voffset_t __Alloverse_State_required[] = { 0 };
typedef flatbuffers_ref_t Alloverse_State_ref_t;
static Alloverse_State_ref_t Alloverse_State_clone(flatbuffers_builder_t *B, Alloverse_State_table_t t);
__flatbuffers_build_table(flatbuffers_, Alloverse_State, 2)

static const flatbuffers_voffset_t __Alloverse_Entity_required[] = { 0 };
typedef flatbuffers_ref_t Alloverse_Entity_ref_t;
static Alloverse_Entity_ref_t Alloverse_Entity_clone(flatbuffers_builder_t *B, Alloverse_Entity_table_t t);
__flatbuffers_build_table(flatbuffers_, Alloverse_Entity, 3)

static const flatbuffers_voffset_t __Alloverse_Components_required[] = { 0 };
typedef flatbuffers_ref_t Alloverse_Components_ref_t;
static Alloverse_Components_ref_t Alloverse_Components_clone(flatbuffers_builder_t *B, Alloverse_Components_table_t t);
__flatbuffers_build_table(flatbuffers_, Alloverse_Components, 5)

static const flatbuffers_voffset_t __Alloverse_TransformComponent_required[] = { 0 };
typedef flatbuffers_ref_t Alloverse_TransformComponent_ref_t;
static Alloverse_TransformComponent_ref_t Alloverse_TransformComponent_clone(flatbuffers_builder_t *B, Alloverse_TransformComponent_table_t t);
__flatbuffers_build_table(flatbuffers_, Alloverse_TransformComponent, 1)

static const flatbuffers_voffset_t __Alloverse_RelationshipsComponent_required[] = { 0 };
typedef flatbuffers_ref_t Alloverse_RelationshipsComponent_ref_t;
static Alloverse_RelationshipsComponent_ref_t Alloverse_RelationshipsComponent_clone(flatbuffers_builder_t *B, Alloverse_RelationshipsComponent_table_t t);
__flatbuffers_build_table(flatbuffers_, Alloverse_RelationshipsComponent, 1)

static const flatbuffers_voffset_t __Alloverse_LiveMediaMetadata_required[] = { 0 };
typedef flatbuffers_ref_t Alloverse_LiveMediaMetadata_ref_t;
static Alloverse_LiveMediaMetadata_ref_t Alloverse_LiveMediaMetadata_clone(flatbuffers_builder_t *B, Alloverse_LiveMediaMetadata_table_t t);
__flatbuffers_build_table(flatbuffers_, Alloverse_LiveMediaMetadata, 4)

static const flatbuffers_voffset_t __Alloverse_LiveMediaComponent_required[] = { 0 };
typedef flatbuffers_ref_t Alloverse_LiveMediaComponent_ref_t;
static Alloverse_LiveMediaComponent_ref_t Alloverse_LiveMediaComponent_clone(flatbuffers_builder_t *B, Alloverse_LiveMediaComponent_table_t t);
__flatbuffers_build_table(flatbuffers_, Alloverse_LiveMediaComponent, 4)

static const flatbuffers_voffset_t __Alloverse_ClockComponent_required[] = { 0 };
typedef flatbuffers_ref_t Alloverse_ClockComponent_ref_t;
static Alloverse_ClockComponent_ref_t Alloverse_ClockComponent_clone(flatbuffers_builder_t *B, Alloverse_ClockComponent_table_t t);
__flatbuffers_build_table(flatbuffers_, Alloverse_ClockComponent, 1)

#define __Alloverse_State_formal_args , uint64_t v0, Alloverse_Entity_vec_ref_t v1
#define __Alloverse_State_call_args , v0, v1
static inline Alloverse_State_ref_t Alloverse_State_create(flatbuffers_builder_t *B __Alloverse_State_formal_args);
__flatbuffers_build_table_prolog(flatbuffers_, Alloverse_State, Alloverse_State_file_identifier, Alloverse_State_type_identifier)

#define __Alloverse_Entity_formal_args , flatbuffers_string_ref_t v0, flatbuffers_string_ref_t v1, Alloverse_Components_ref_t v2
#define __Alloverse_Entity_call_args , v0, v1, v2
static inline Alloverse_Entity_ref_t Alloverse_Entity_create(flatbuffers_builder_t *B __Alloverse_Entity_formal_args);
__flatbuffers_build_table_prolog(flatbuffers_, Alloverse_Entity, Alloverse_Entity_file_identifier, Alloverse_Entity_type_identifier)

#define __Alloverse_Components_formal_args ,\
  Alloverse_TransformComponent_ref_t v0, Alloverse_RelationshipsComponent_ref_t v1, Alloverse_LiveMediaComponent_ref_t v2, Alloverse_ClockComponent_ref_t v3, flatbuffers_uint8_vec_ref_t v4
#define __Alloverse_Components_call_args ,\
  v0, v1, v2, v3, v4
static inline Alloverse_Components_ref_t Alloverse_Components_create(flatbuffers_builder_t *B __Alloverse_Components_formal_args);
__flatbuffers_build_table_prolog(flatbuffers_, Alloverse_Components, Alloverse_Components_file_identifier, Alloverse_Components_type_identifier)

#define __Alloverse_TransformComponent_formal_args , Alloverse_Mat4_t *v0
#define __Alloverse_TransformComponent_call_args , v0
static inline Alloverse_TransformComponent_ref_t Alloverse_TransformComponent_create(flatbuffers_builder_t *B __Alloverse_TransformComponent_formal_args);
__flatbuffers_build_table_prolog(flatbuffers_, Alloverse_TransformComponent, Alloverse_TransformComponent_file_identifier, Alloverse_TransformComponent_type_identifier)

#define __Alloverse_RelationshipsComponent_formal_args , flatbuffers_string_ref_t v0
#define __Alloverse_RelationshipsComponent_call_args , v0
static inline Alloverse_RelationshipsComponent_ref_t Alloverse_RelationshipsComponent_create(flatbuffers_builder_t *B __Alloverse_RelationshipsComponent_formal_args);
__flatbuffers_build_table_prolog(flatbuffers_, Alloverse_RelationshipsComponent, Alloverse_RelationshipsComponent_file_identifier, Alloverse_RelationshipsComponent_type_identifier)

#define __Alloverse_LiveMediaMetadata_formal_args , int32_t v0, flatbuffers_string_ref_t v1, int32_t v2, int32_t v3
#define __Alloverse_LiveMediaMetadata_call_args , v0, v1, v2, v3
static inline Alloverse_LiveMediaMetadata_ref_t Alloverse_LiveMediaMetadata_create(flatbuffers_builder_t *B __Alloverse_LiveMediaMetadata_formal_args);
__flatbuffers_build_table_prolog(flatbuffers_, Alloverse_LiveMediaMetadata, Alloverse_LiveMediaMetadata_file_identifier, Alloverse_LiveMediaMetadata_type_identifier)

#define __Alloverse_LiveMediaComponent_formal_args , int32_t v0, flatbuffers_string_ref_t v1, flatbuffers_string_ref_t v2, Alloverse_LiveMediaMetadata_ref_t v3
#define __Alloverse_LiveMediaComponent_call_args , v0, v1, v2, v3
static inline Alloverse_LiveMediaComponent_ref_t Alloverse_LiveMediaComponent_create(flatbuffers_builder_t *B __Alloverse_LiveMediaComponent_formal_args);
__flatbuffers_build_table_prolog(flatbuffers_, Alloverse_LiveMediaComponent, Alloverse_LiveMediaComponent_file_identifier, Alloverse_LiveMediaComponent_type_identifier)

#define __Alloverse_ClockComponent_formal_args , double v0
#define __Alloverse_ClockComponent_call_args , v0
static inline Alloverse_ClockComponent_ref_t Alloverse_ClockComponent_create(flatbuffers_builder_t *B __Alloverse_ClockComponent_formal_args);
__flatbuffers_build_table_prolog(flatbuffers_, Alloverse_ClockComponent, Alloverse_ClockComponent_file_identifier, Alloverse_ClockComponent_type_identifier)

__flatbuffers_build_scalar_field(0, flatbuffers_, Alloverse_State_revision, flatbuffers_uint64, uint64_t, 8, 8, UINT64_C(0), Alloverse_State)
__flatbuffers_build_table_vector_field(1, flatbuffers_, Alloverse_State_entities, Alloverse_Entity, Alloverse_State)

static inline Alloverse_State_ref_t Alloverse_State_create(flatbuffers_builder_t *B __Alloverse_State_formal_args)
{
    if (Alloverse_State_start(B)
        || Alloverse_State_revision_add(B, v0)
        || Alloverse_State_entities_add(B, v1)) {
        return 0;
    }
    return Alloverse_State_end(B);
}

static Alloverse_State_ref_t Alloverse_State_clone(flatbuffers_builder_t *B, Alloverse_State_table_t t)
{
    __flatbuffers_memoize_begin(B, t);
    if (Alloverse_State_start(B)
        || Alloverse_State_revision_pick(B, t)
        || Alloverse_State_entities_pick(B, t)) {
        return 0;
    }
    __flatbuffers_memoize_end(B, t, Alloverse_State_end(B));
}

__flatbuffers_build_string_field(0, flatbuffers_, Alloverse_Entity_id, Alloverse_Entity)
__flatbuffers_build_string_field(1, flatbuffers_, Alloverse_Entity_owner_agent_id, Alloverse_Entity)
__flatbuffers_build_table_field(2, flatbuffers_, Alloverse_Entity_components, Alloverse_Components, Alloverse_Entity)

static inline Alloverse_Entity_ref_t Alloverse_Entity_create(flatbuffers_builder_t *B __Alloverse_Entity_formal_args)
{
    if (Alloverse_Entity_start(B)
        || Alloverse_Entity_id_add(B, v0)
        || Alloverse_Entity_owner_agent_id_add(B, v1)
        || Alloverse_Entity_components_add(B, v2)) {
        return 0;
    }
    return Alloverse_Entity_end(B);
}

static Alloverse_Entity_ref_t Alloverse_Entity_clone(flatbuffers_builder_t *B, Alloverse_Entity_table_t t)
{
    __flatbuffers_memoize_begin(B, t);
    if (Alloverse_Entity_start(B)
        || Alloverse_Entity_id_pick(B, t)
        || Alloverse_Entity_owner_agent_id_pick(B, t)
        || Alloverse_Entity_components_pick(B, t)) {
        return 0;
    }
    __flatbuffers_memoize_end(B, t, Alloverse_Entity_end(B));
}

__flatbuffers_build_table_field(0, flatbuffers_, Alloverse_Components_transform, Alloverse_TransformComponent, Alloverse_Components)
__flatbuffers_build_table_field(1, flatbuffers_, Alloverse_Components_relationships, Alloverse_RelationshipsComponent, Alloverse_Components)
__flatbuffers_build_table_field(2, flatbuffers_, Alloverse_Components_live_media, Alloverse_LiveMediaComponent, Alloverse_Components)
__flatbuffers_build_table_field(3, flatbuffers_, Alloverse_Components_clock, Alloverse_ClockComponent, Alloverse_Components)
__flatbuffers_build_vector_field(4, flatbuffers_, Alloverse_Components_flex, flatbuffers_uint8, uint8_t, Alloverse_Components)

static inline Alloverse_Components_ref_t Alloverse_Components_create(flatbuffers_builder_t *B __Alloverse_Components_formal_args)
{
    if (Alloverse_Components_start(B)
        || Alloverse_Components_transform_add(B, v0)
        || Alloverse_Components_relationships_add(B, v1)
        || Alloverse_Components_live_media_add(B, v2)
        || Alloverse_Components_clock_add(B, v3)
        || Alloverse_Components_flex_add(B, v4)) {
        return 0;
    }
    return Alloverse_Components_end(B);
}

static Alloverse_Components_ref_t Alloverse_Components_clone(flatbuffers_builder_t *B, Alloverse_Components_table_t t)
{
    __flatbuffers_memoize_begin(B, t);
    if (Alloverse_Components_start(B)
        || Alloverse_Components_transform_pick(B, t)
        || Alloverse_Components_relationships_pick(B, t)
        || Alloverse_Components_live_media_pick(B, t)
        || Alloverse_Components_clock_pick(B, t)
        || Alloverse_Components_flex_pick(B, t)) {
        return 0;
    }
    __flatbuffers_memoize_end(B, t, Alloverse_Components_end(B));
}

__flatbuffers_build_struct_field(0, flatbuffers_, Alloverse_TransformComponent_matrix, Alloverse_Mat4, 64, 4, Alloverse_TransformComponent)

static inline Alloverse_TransformComponent_ref_t Alloverse_TransformComponent_create(flatbuffers_builder_t *B __Alloverse_TransformComponent_formal_args)
{
    if (Alloverse_TransformComponent_start(B)
        || Alloverse_TransformComponent_matrix_add(B, v0)) {
        return 0;
    }
    return Alloverse_TransformComponent_end(B);
}

static Alloverse_TransformComponent_ref_t Alloverse_TransformComponent_clone(flatbuffers_builder_t *B, Alloverse_TransformComponent_table_t t)
{
    __flatbuffers_memoize_begin(B, t);
    if (Alloverse_TransformComponent_start(B)
        || Alloverse_TransformComponent_matrix_pick(B, t)) {
        return 0;
    }
    __flatbuffers_memoize_end(B, t, Alloverse_TransformComponent_end(B));
}

__flatbuffers_build_string_field(0, flatbuffers_, Alloverse_RelationshipsComponent_parent, Alloverse_RelationshipsComponent)

static inline Alloverse_RelationshipsComponent_ref_t Alloverse_RelationshipsComponent_create(flatbuffers_builder_t *B __Alloverse_RelationshipsComponent_formal_args)
{
    if (Alloverse_RelationshipsComponent_start(B)
        || Alloverse_RelationshipsComponent_parent_add(B, v0)) {
        return 0;
    }
    return Alloverse_RelationshipsComponent_end(B);
}

static Alloverse_RelationshipsComponent_ref_t Alloverse_RelationshipsComponent_clone(flatbuffers_builder_t *B, Alloverse_RelationshipsComponent_table_t t)
{
    __flatbuffers_memoize_begin(B, t);
    if (Alloverse_RelationshipsComponent_start(B)
        || Alloverse_RelationshipsComponent_parent_pick(B, t)) {
        return 0;
    }
    __flatbuffers_memoize_end(B, t, Alloverse_RelationshipsComponent_end(B));
}

__flatbuffers_build_scalar_field(0, flatbuffers_, Alloverse_LiveMediaMetadata_sample_rate, flatbuffers_int32, int32_t, 4, 4, INT32_C(0), Alloverse_LiveMediaMetadata)
__flatbuffers_build_string_field(1, flatbuffers_, Alloverse_LiveMediaMetadata_channel_layout, Alloverse_LiveMediaMetadata)
__flatbuffers_build_scalar_field(2, flatbuffers_, Alloverse_LiveMediaMetadata_width, flatbuffers_int32, int32_t, 4, 4, INT32_C(0), Alloverse_LiveMediaMetadata)
__flatbuffers_build_scalar_field(3, flatbuffers_, Alloverse_LiveMediaMetadata_height, flatbuffers_int32, int32_t, 4, 4, INT32_C(0), Alloverse_LiveMediaMetadata)

static inline Alloverse_LiveMediaMetadata_ref_t Alloverse_LiveMediaMetadata_create(flatbuffers_builder_t *B __Alloverse_LiveMediaMetadata_formal_args)
{
    if (Alloverse_LiveMediaMetadata_start(B)
        || Alloverse_LiveMediaMetadata_sample_rate_add(B, v0)
        || Alloverse_LiveMediaMetadata_channel_layout_add(B, v1)
        || Alloverse_LiveMediaMetadata_width_add(B, v2)
        || Alloverse_LiveMediaMetadata_height_add(B, v3)) {
        return 0;
    }
    return Alloverse_LiveMediaMetadata_end(B);
}

static Alloverse_LiveMediaMetadata_ref_t Alloverse_LiveMediaMetadata_clone(flatbuffers_builder_t *B, Alloverse_LiveMediaMetadata_table_t t)
{
    __flatbuffers_memoize_begin(B, t);
    if (Alloverse_LiveMediaMetadata_start(B)
        || Alloverse_LiveMediaMetadata_sample_rate_pick(B, t)
        || Alloverse_LiveMediaMetadata_channel_layout_pick(B, t)
        || Alloverse_LiveMediaMetadata_width_pick(B, t)
        || Alloverse_LiveMediaMetadata_height_pick(B, t)) {
        return 0;
    }
    __flatbuffers_memoize_end(B, t, Alloverse_LiveMediaMetadata_end(B));
}

__flatbuffers_build_scalar_field(0, flatbuffers_, Alloverse_LiveMediaComponent_track_id, flatbuffers_int32, int32_t, 4, 4, INT32_C(0), Alloverse_LiveMediaComponent)
__flatbuffers_build_string_field(1, flatbuffers_, Alloverse_LiveMediaComponent_type, Alloverse_LiveMediaComponent)
__flatbuffers_build_string_field(2, flatbuffers_, Alloverse_LiveMediaComponent_format, Alloverse_LiveMediaComponent)
__flatbuffers_build_table_field(3, flatbuffers_, Alloverse_LiveMediaComponent_metadata, Alloverse_LiveMediaMetadata, Alloverse_LiveMediaComponent)

static inline Alloverse_LiveMediaComponent_ref_t Alloverse_LiveMediaComponent_create(flatbuffers_builder_t *B __Alloverse_LiveMediaComponent_formal_args)
{
    if (Alloverse_LiveMediaComponent_start(B)
        || Alloverse_LiveMediaComponent_track_id_add(B, v0)
        || Alloverse_LiveMediaComponent_type_add(B, v1)
        || Alloverse_LiveMediaComponent_format_add(B, v2)
        || Alloverse_LiveMediaComponent_metadata_add(B, v3)) {
        return 0;
    }
    return Alloverse_LiveMediaComponent_end(B);
}

static Alloverse_LiveMediaComponent_ref_t Alloverse_LiveMediaComponent_clone(flatbuffers_builder_t *B, Alloverse_LiveMediaComponent_table_t t)
{
    __flatbuffers_memoize_begin(B, t);
    if (Alloverse_LiveMediaComponent_start(B)
        || Alloverse_LiveMediaComponent_track_id_pick(B, t)
        || Alloverse_LiveMediaComponent_type_pick(B, t)
        || Alloverse_LiveMediaComponent_format_pick(B, t)
        || Alloverse_LiveMediaComponent_metadata_pick(B, t)) {
        return 0;
    }
    __flatbuffers_memoize_end(B, t, Alloverse_LiveMediaComponent_end(B));
}

__flatbuffers_build_scalar_field(0, flatbuffers_, Alloverse_ClockComponent_time, flatbuffers_double, double, 8, 8, 0.0000000000000000, Alloverse_ClockComponent)

static inline Alloverse_ClockComponent_ref_t Alloverse_ClockComponent_create(flatbuffers_builder_t *B __Alloverse_ClockComponent_formal_args)
{
    if (Alloverse_ClockComponent_start(B)
        || Alloverse_ClockComponent_time_add(B, v0)) {
        return 0;
    }
    return Alloverse_ClockComponent_end(B);
}

static Alloverse_ClockComponent_ref_t Alloverse_ClockComponent_clone(flatbuffers_builder_t *B, Alloverse_ClockComponent_table_t t)
{
    __flatbuffers_memoize_begin(B, t);
    if (Alloverse_ClockComponent_start(B)
        || Alloverse_ClockComponent_time_pick(B, t)) {
        return 0;
    }
    __flatbuffers_memoize_end(B, t, Alloverse_ClockComponent_end(B));
}

#include "flatcc/flatcc_epilogue.h"
#endif /* ALLOVERSE_BUILDER_H */
