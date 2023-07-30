/* Generated by the protocol buffer compiler.  DO NOT EDIT! */
/* Generated from: proto/toniebox.pb.rtnl.proto */

/* Do not generate deprecated warnings for self */
#ifndef PROTOBUF_C__NO_DEPRECATED
#define PROTOBUF_C__NO_DEPRECATED
#endif

#include "proto/toniebox.pb.rtnl.pb-c.h"
void   tonie_rtnl_rpc__init
                     (TonieRtnlRPC         *message)
{
  static const TonieRtnlRPC init_value = TONIE_RTNL_RPC__INIT;
  *message = init_value;
}
size_t tonie_rtnl_rpc__get_packed_size
                     (const TonieRtnlRPC *message)
{
  assert(message->base.descriptor == &tonie_rtnl_rpc__descriptor);
  return protobuf_c_message_get_packed_size ((const ProtobufCMessage*)(message));
}
size_t tonie_rtnl_rpc__pack
                     (const TonieRtnlRPC *message,
                      uint8_t       *out)
{
  assert(message->base.descriptor == &tonie_rtnl_rpc__descriptor);
  return protobuf_c_message_pack ((const ProtobufCMessage*)message, out);
}
size_t tonie_rtnl_rpc__pack_to_buffer
                     (const TonieRtnlRPC *message,
                      ProtobufCBuffer *buffer)
{
  assert(message->base.descriptor == &tonie_rtnl_rpc__descriptor);
  return protobuf_c_message_pack_to_buffer ((const ProtobufCMessage*)message, buffer);
}
TonieRtnlRPC *
       tonie_rtnl_rpc__unpack
                     (ProtobufCAllocator  *allocator,
                      size_t               len,
                      const uint8_t       *data)
{
  return (TonieRtnlRPC *)
     protobuf_c_message_unpack (&tonie_rtnl_rpc__descriptor,
                                allocator, len, data);
}
void   tonie_rtnl_rpc__free_unpacked
                     (TonieRtnlRPC *message,
                      ProtobufCAllocator *allocator)
{
  if(!message)
    return;
  assert(message->base.descriptor == &tonie_rtnl_rpc__descriptor);
  protobuf_c_message_free_unpacked ((ProtobufCMessage*)message, allocator);
}
void   tonie_rtnl_log2__init
                     (TonieRtnlLog2         *message)
{
  static const TonieRtnlLog2 init_value = TONIE_RTNL_LOG2__INIT;
  *message = init_value;
}
size_t tonie_rtnl_log2__get_packed_size
                     (const TonieRtnlLog2 *message)
{
  assert(message->base.descriptor == &tonie_rtnl_log2__descriptor);
  return protobuf_c_message_get_packed_size ((const ProtobufCMessage*)(message));
}
size_t tonie_rtnl_log2__pack
                     (const TonieRtnlLog2 *message,
                      uint8_t       *out)
{
  assert(message->base.descriptor == &tonie_rtnl_log2__descriptor);
  return protobuf_c_message_pack ((const ProtobufCMessage*)message, out);
}
size_t tonie_rtnl_log2__pack_to_buffer
                     (const TonieRtnlLog2 *message,
                      ProtobufCBuffer *buffer)
{
  assert(message->base.descriptor == &tonie_rtnl_log2__descriptor);
  return protobuf_c_message_pack_to_buffer ((const ProtobufCMessage*)message, buffer);
}
TonieRtnlLog2 *
       tonie_rtnl_log2__unpack
                     (ProtobufCAllocator  *allocator,
                      size_t               len,
                      const uint8_t       *data)
{
  return (TonieRtnlLog2 *)
     protobuf_c_message_unpack (&tonie_rtnl_log2__descriptor,
                                allocator, len, data);
}
void   tonie_rtnl_log2__free_unpacked
                     (TonieRtnlLog2 *message,
                      ProtobufCAllocator *allocator)
{
  if(!message)
    return;
  assert(message->base.descriptor == &tonie_rtnl_log2__descriptor);
  protobuf_c_message_free_unpacked ((ProtobufCMessage*)message, allocator);
}
void   tonie_rtnl_log3__init
                     (TonieRtnlLog3         *message)
{
  static const TonieRtnlLog3 init_value = TONIE_RTNL_LOG3__INIT;
  *message = init_value;
}
size_t tonie_rtnl_log3__get_packed_size
                     (const TonieRtnlLog3 *message)
{
  assert(message->base.descriptor == &tonie_rtnl_log3__descriptor);
  return protobuf_c_message_get_packed_size ((const ProtobufCMessage*)(message));
}
size_t tonie_rtnl_log3__pack
                     (const TonieRtnlLog3 *message,
                      uint8_t       *out)
{
  assert(message->base.descriptor == &tonie_rtnl_log3__descriptor);
  return protobuf_c_message_pack ((const ProtobufCMessage*)message, out);
}
size_t tonie_rtnl_log3__pack_to_buffer
                     (const TonieRtnlLog3 *message,
                      ProtobufCBuffer *buffer)
{
  assert(message->base.descriptor == &tonie_rtnl_log3__descriptor);
  return protobuf_c_message_pack_to_buffer ((const ProtobufCMessage*)message, buffer);
}
TonieRtnlLog3 *
       tonie_rtnl_log3__unpack
                     (ProtobufCAllocator  *allocator,
                      size_t               len,
                      const uint8_t       *data)
{
  return (TonieRtnlLog3 *)
     protobuf_c_message_unpack (&tonie_rtnl_log3__descriptor,
                                allocator, len, data);
}
void   tonie_rtnl_log3__free_unpacked
                     (TonieRtnlLog3 *message,
                      ProtobufCAllocator *allocator)
{
  if(!message)
    return;
  assert(message->base.descriptor == &tonie_rtnl_log3__descriptor);
  protobuf_c_message_free_unpacked ((ProtobufCMessage*)message, allocator);
}
static const ProtobufCFieldDescriptor tonie_rtnl_rpc__field_descriptors[2] =
{
  {
    "log2",
    2,
    PROTOBUF_C_LABEL_OPTIONAL,
    PROTOBUF_C_TYPE_MESSAGE,
    0,   /* quantifier_offset */
    offsetof(TonieRtnlRPC, log2),
    &tonie_rtnl_log2__descriptor,
    NULL,
    0,             /* flags */
    0,NULL,NULL    /* reserved1,reserved2, etc */
  },
  {
    "log3",
    3,
    PROTOBUF_C_LABEL_OPTIONAL,
    PROTOBUF_C_TYPE_MESSAGE,
    0,   /* quantifier_offset */
    offsetof(TonieRtnlRPC, log3),
    &tonie_rtnl_log3__descriptor,
    NULL,
    0,             /* flags */
    0,NULL,NULL    /* reserved1,reserved2, etc */
  },
};
static const unsigned tonie_rtnl_rpc__field_indices_by_name[] = {
  0,   /* field[0] = log2 */
  1,   /* field[1] = log3 */
};
static const ProtobufCIntRange tonie_rtnl_rpc__number_ranges[1 + 1] =
{
  { 2, 0 },
  { 0, 2 }
};
const ProtobufCMessageDescriptor tonie_rtnl_rpc__descriptor =
{
  PROTOBUF_C__MESSAGE_DESCRIPTOR_MAGIC,
  "TonieRtnlRPC",
  "TonieRtnlRPC",
  "TonieRtnlRPC",
  "",
  sizeof(TonieRtnlRPC),
  2,
  tonie_rtnl_rpc__field_descriptors,
  tonie_rtnl_rpc__field_indices_by_name,
  1,  tonie_rtnl_rpc__number_ranges,
  (ProtobufCMessageInit) tonie_rtnl_rpc__init,
  NULL,NULL,NULL    /* reserved[123] */
};
static const ProtobufCFieldDescriptor tonie_rtnl_log2__field_descriptors[8] =
{
  {
    "uptime",
    1,
    PROTOBUF_C_LABEL_REQUIRED,
    PROTOBUF_C_TYPE_FIXED64,
    0,   /* quantifier_offset */
    offsetof(TonieRtnlLog2, uptime),
    NULL,
    NULL,
    0,             /* flags */
    0,NULL,NULL    /* reserved1,reserved2, etc */
  },
  {
    "sequence",
    2,
    PROTOBUF_C_LABEL_REQUIRED,
    PROTOBUF_C_TYPE_UINT32,
    0,   /* quantifier_offset */
    offsetof(TonieRtnlLog2, sequence),
    NULL,
    NULL,
    0,             /* flags */
    0,NULL,NULL    /* reserved1,reserved2, etc */
  },
  {
    "field3",
    3,
    PROTOBUF_C_LABEL_REQUIRED,
    PROTOBUF_C_TYPE_UINT32,
    0,   /* quantifier_offset */
    offsetof(TonieRtnlLog2, field3),
    NULL,
    NULL,
    0,             /* flags */
    0,NULL,NULL    /* reserved1,reserved2, etc */
  },
  {
    "function_group",
    4,
    PROTOBUF_C_LABEL_REQUIRED,
    PROTOBUF_C_TYPE_UINT32,
    0,   /* quantifier_offset */
    offsetof(TonieRtnlLog2, function_group),
    NULL,
    NULL,
    0,             /* flags */
    0,NULL,NULL    /* reserved1,reserved2, etc */
  },
  {
    "function",
    5,
    PROTOBUF_C_LABEL_REQUIRED,
    PROTOBUF_C_TYPE_UINT32,
    0,   /* quantifier_offset */
    offsetof(TonieRtnlLog2, function),
    NULL,
    NULL,
    0,             /* flags */
    0,NULL,NULL    /* reserved1,reserved2, etc */
  },
  {
    "field6",
    6,
    PROTOBUF_C_LABEL_REQUIRED,
    PROTOBUF_C_TYPE_BYTES,
    0,   /* quantifier_offset */
    offsetof(TonieRtnlLog2, field6),
    NULL,
    NULL,
    0,             /* flags */
    0,NULL,NULL    /* reserved1,reserved2, etc */
  },
  {
    "field8",
    8,
    PROTOBUF_C_LABEL_OPTIONAL,
    PROTOBUF_C_TYPE_FIXED32,
    offsetof(TonieRtnlLog2, has_field8),
    offsetof(TonieRtnlLog2, field8),
    NULL,
    NULL,
    0,             /* flags */
    0,NULL,NULL    /* reserved1,reserved2, etc */
  },
  {
    "field9",
    9,
    PROTOBUF_C_LABEL_OPTIONAL,
    PROTOBUF_C_TYPE_BYTES,
    offsetof(TonieRtnlLog2, has_field9),
    offsetof(TonieRtnlLog2, field9),
    NULL,
    NULL,
    0,             /* flags */
    0,NULL,NULL    /* reserved1,reserved2, etc */
  },
};
static const unsigned tonie_rtnl_log2__field_indices_by_name[] = {
  2,   /* field[2] = field3 */
  5,   /* field[5] = field6 */
  6,   /* field[6] = field8 */
  7,   /* field[7] = field9 */
  4,   /* field[4] = function */
  3,   /* field[3] = function_group */
  1,   /* field[1] = sequence */
  0,   /* field[0] = uptime */
};
static const ProtobufCIntRange tonie_rtnl_log2__number_ranges[2 + 1] =
{
  { 1, 0 },
  { 8, 6 },
  { 0, 8 }
};
const ProtobufCMessageDescriptor tonie_rtnl_log2__descriptor =
{
  PROTOBUF_C__MESSAGE_DESCRIPTOR_MAGIC,
  "TonieRtnlLog2",
  "TonieRtnlLog2",
  "TonieRtnlLog2",
  "",
  sizeof(TonieRtnlLog2),
  8,
  tonie_rtnl_log2__field_descriptors,
  tonie_rtnl_log2__field_indices_by_name,
  2,  tonie_rtnl_log2__number_ranges,
  (ProtobufCMessageInit) tonie_rtnl_log2__init,
  NULL,NULL,NULL    /* reserved[123] */
};
static const ProtobufCFieldDescriptor tonie_rtnl_log3__field_descriptors[2] =
{
  {
    "datetime",
    1,
    PROTOBUF_C_LABEL_REQUIRED,
    PROTOBUF_C_TYPE_FIXED32,
    0,   /* quantifier_offset */
    offsetof(TonieRtnlLog3, datetime),
    NULL,
    NULL,
    0,             /* flags */
    0,NULL,NULL    /* reserved1,reserved2, etc */
  },
  {
    "field2",
    2,
    PROTOBUF_C_LABEL_REQUIRED,
    PROTOBUF_C_TYPE_UINT32,
    0,   /* quantifier_offset */
    offsetof(TonieRtnlLog3, field2),
    NULL,
    NULL,
    0,             /* flags */
    0,NULL,NULL    /* reserved1,reserved2, etc */
  },
};
static const unsigned tonie_rtnl_log3__field_indices_by_name[] = {
  0,   /* field[0] = datetime */
  1,   /* field[1] = field2 */
};
static const ProtobufCIntRange tonie_rtnl_log3__number_ranges[1 + 1] =
{
  { 1, 0 },
  { 0, 2 }
};
const ProtobufCMessageDescriptor tonie_rtnl_log3__descriptor =
{
  PROTOBUF_C__MESSAGE_DESCRIPTOR_MAGIC,
  "TonieRtnlLog3",
  "TonieRtnlLog3",
  "TonieRtnlLog3",
  "",
  sizeof(TonieRtnlLog3),
  2,
  tonie_rtnl_log3__field_descriptors,
  tonie_rtnl_log3__field_indices_by_name,
  1,  tonie_rtnl_log3__number_ranges,
  (ProtobufCMessageInit) tonie_rtnl_log3__init,
  NULL,NULL,NULL    /* reserved[123] */
};
