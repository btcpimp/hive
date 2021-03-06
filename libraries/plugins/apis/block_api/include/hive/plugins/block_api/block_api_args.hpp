#pragma once
#include <hive/plugins/block_api/block_api_objects.hpp>

#include <hive/protocol/types.hpp>
#include <hive/protocol/transaction.hpp>
#include <hive/protocol/block_header.hpp>

#include <hive/plugins/json_rpc/utility.hpp>

namespace hive { namespace plugins { namespace block_api {

/* get_block_header */

struct get_block_header_args
{
  uint32_t block_num;
};

struct get_block_header_return
{
  optional< block_header > header;
};

/* get_block */
struct get_block_args
{
  uint32_t block_num;
};

struct get_block_return
{
  optional< api_signed_block_object > block;
};

} } } // hive::block_api

FC_REFLECT( hive::plugins::block_api::get_block_header_args,
  (block_num) )

FC_REFLECT( hive::plugins::block_api::get_block_header_return,
  (header) )

FC_REFLECT( hive::plugins::block_api::get_block_args,
  (block_num) )

FC_REFLECT( hive::plugins::block_api::get_block_return,
  (block) )

