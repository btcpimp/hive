#include <hive/plugins/account_history_api/account_history_api_plugin.hpp>
#include <hive/plugins/account_history_api/account_history_api.hpp>

#include <hive/plugins/account_history_rocksdb/account_history_rocksdb_plugin.hpp>

namespace hive { namespace plugins { namespace account_history {

namespace detail {

class abstract_account_history_api_impl
{
  public:
    abstract_account_history_api_impl() : _db( appbase::app().get_plugin< hive::plugins::chain::chain_plugin >().db() ) {}
    virtual ~abstract_account_history_api_impl() {}

    virtual get_ops_in_block_return get_ops_in_block( const get_ops_in_block_args& ) = 0;
    virtual get_transaction_return get_transaction( const get_transaction_args& ) = 0;
    virtual get_account_history_return get_account_history( const get_account_history_args& ) = 0;
    virtual enum_virtual_ops_return enum_virtual_ops( const enum_virtual_ops_args& ) = 0;

    chain::database& _db;
};

class account_history_api_chainbase_impl : public abstract_account_history_api_impl
{
  public:
    account_history_api_chainbase_impl() : abstract_account_history_api_impl() {}
    ~account_history_api_chainbase_impl() {}

    get_ops_in_block_return get_ops_in_block( const get_ops_in_block_args& ) override;
    get_transaction_return get_transaction( const get_transaction_args& ) override;
    get_account_history_return get_account_history( const get_account_history_args& ) override;
    enum_virtual_ops_return enum_virtual_ops( const enum_virtual_ops_args& ) override;
};

DEFINE_API_IMPL( account_history_api_chainbase_impl, get_ops_in_block )
{
  return _db.with_read_lock( [&]()
  {
    const auto& idx = _db.get_index< chain::operation_index, chain::by_location >();
    auto itr = idx.lower_bound( args.block_num );

    get_ops_in_block_return result;

    while( itr != idx.end() && itr->block == args.block_num )
    {
      api_operation_object temp = *itr;
      if( !args.only_virtual || is_virtual_operation( temp.op ) )
        result.ops.emplace( std::move( temp ) );
      ++itr;
    }

    return result;
  });
}

DEFINE_API_IMPL( account_history_api_chainbase_impl, get_transaction )
{
#ifdef SKIP_BY_TX_ID
  FC_ASSERT( false, "This node's operator has disabled operation indexing by transaction_id" );
#else

  return _db.with_read_lock( [&]()
  {
    get_transaction_return result;

    const auto& idx = _db.get_index< chain::operation_index, chain::by_transaction_id >();
    auto itr = idx.lower_bound( args.id );
    if( itr != idx.end() && itr->trx_id == args.id )
    {
      auto blk = _db.fetch_block_by_number( itr->block );
      FC_ASSERT( blk.valid() );
      FC_ASSERT( blk->transactions.size() > itr->trx_in_block );
      result = blk->transactions[itr->trx_in_block];
      result.block_num       = itr->block;
      result.transaction_num = itr->trx_in_block;
    }
    else
    {
      FC_ASSERT( false, "Unknown Transaction ${t}", ("t",args.id) );
    }

    return result;
  });
#endif
}

DEFINE_API_IMPL( account_history_api_chainbase_impl, get_account_history )
{
  FC_ASSERT( args.limit <= 10000, "limit of ${l} is greater than maxmimum allowed", ("l",args.limit) );
  FC_ASSERT( args.start >= args.limit, "start must be greater than limit" );

  return _db.with_read_lock( [&]()
  {
    const auto& idx = _db.get_index< chain::account_history_index, chain::by_account >();
    auto itr = idx.lower_bound( boost::make_tuple( args.account, args.start ) );
    uint32_t n = 0;

    get_account_history_return result;
    while( true )
    {
      if( itr == idx.end() )
        break;
      if( itr->account != args.account )
        break;
      if( n >= args.limit )
        break;
      result.history[ itr->sequence ] = _db.get( itr->op );
      ++itr;
      ++n;
    }

    return result;
  });
}

DEFINE_API_IMPL( account_history_api_chainbase_impl, enum_virtual_ops )
{
  FC_ASSERT( false, "This API is not supported for account history backed by Chainbase" );
}

class account_history_api_rocksdb_impl : public abstract_account_history_api_impl
{
  public:
    account_history_api_rocksdb_impl() :
      abstract_account_history_api_impl(), _dataSource( appbase::app().get_plugin< hive::plugins::account_history_rocksdb::account_history_rocksdb_plugin >() ) {}
    ~account_history_api_rocksdb_impl() {}

    get_ops_in_block_return get_ops_in_block( const get_ops_in_block_args& ) override;
    get_transaction_return get_transaction( const get_transaction_args& ) override;
    get_account_history_return get_account_history( const get_account_history_args& ) override;
    enum_virtual_ops_return enum_virtual_ops( const enum_virtual_ops_args& ) override;

    const account_history_rocksdb::account_history_rocksdb_plugin& _dataSource;
};

DEFINE_API_IMPL( account_history_api_rocksdb_impl, get_ops_in_block )
{
  get_ops_in_block_return result;
  _dataSource.find_operations_by_block(args.block_num,
    [&result, &args](const account_history_rocksdb::rocksdb_operation_object& op)
    {
      api_operation_object temp(op);
      if( !args.only_virtual || is_virtual_operation( temp.op ) )
        result.ops.emplace(std::move(temp));
    }
  );
  return result;
}

DEFINE_API_IMPL( account_history_api_rocksdb_impl, get_account_history )
{
  FC_ASSERT( args.limit <= 10000, "limit of ${l} is greater than maxmimum allowed", ("l",args.limit) );
  FC_ASSERT( args.start >= args.limit, "start must be greater than limit" );

  get_account_history_return result;

  _dataSource.find_account_history_data(args.account, args.start, args.limit,
    [&result](unsigned int sequence, const account_history_rocksdb::rocksdb_operation_object& op)
    {
      result.history[sequence] = api_operation_object( op );
    });

  return result;
}

DEFINE_API_IMPL( account_history_api_rocksdb_impl, get_transaction )
{
#ifdef SKIP_BY_TX_ID
  FC_ASSERT(false, "This node's operator has disabled operation indexing by transaction_id");
#else
  uint32_t blockNo = 0;
  uint32_t txInBlock = 0;

  if(_dataSource.find_transaction_info(args.id, &blockNo, &txInBlock))
    {
    get_transaction_return result;
    _db.with_read_lock([this, blockNo, txInBlock, &result]()
    {
    auto blk = _db.fetch_block_by_number(blockNo);
    FC_ASSERT(blk.valid());
    FC_ASSERT(blk->transactions.size() > txInBlock);
    result = blk->transactions[txInBlock];
    result.block_num = blockNo;
    result.transaction_num = txInBlock;
    });

    return result;
    }
  else
    {
    FC_ASSERT(false, "Unknown Transaction ${t}", ("t", args.id));
    }
#endif

}

#define GET_NAME( r, data, CLASS_NAME ) \
    void operator()( const hive::protocol::CLASS_NAME& op ) { name = BOOST_PP_STRINGIZE(CLASS_NAME); }

#define GET_NAMES( CLASS_NAMES ) \
	BOOST_PP_SEQ_FOR_EACH( GET_NAME, _, CLASS_NAMES )

struct name_visitor
{
  std::string name;

  typedef void result_type;

  template< typename T >
  void operator()( const T& ) { name = ""; }

  GET_NAMES( (fill_convert_request_operation)(author_reward_operation)(curation_reward_operation)
  (comment_reward_operation)(liquidity_reward_operation)(interest_operation)
  (fill_vesting_withdraw_operation)(fill_order_operation)(shutdown_witness_operation)
  (fill_transfer_from_savings_operation)(hardfork_operation)(comment_payout_update_operation)
  (return_vesting_delegation_operation)(comment_benefactor_reward_operation)(producer_reward_operation)
  (clear_null_account_balance_operation)(proposal_pay_operation)(sps_fund_operation)
  (hardfork_hive_operation)(hardfork_hive_restore_operation)(delayed_voting_operation)
  (consolidate_treasury_balance_operation)(effective_comment_vote_operation) )
};

DEFINE_API_IMPL( account_history_api_rocksdb_impl, enum_virtual_ops)
{
  enum_virtual_ops_return result;

    std::pair< uint32_t, uint32_t > next_values = _dataSource.enum_operations_from_block_range(args.block_range_begin,
    args.block_range_end, args.operation_begin, args.limit,
    [ &result, &args ](const account_history_rocksdb::rocksdb_operation_object& op)
    {
      if( args.filter.valid() )
      {
        api_operation_object _api_obj( op );

        name_visitor _name_visitor;
        _api_obj.op.visit( _name_visitor );

        if( _name_visitor.name.size() && args.filter->find( _name_visitor.name ) != args.filter->end() )
        {
          result.ops.emplace_back(api_operation_object(op));
          return true;
        }
        else
          return false;
      }
      else
      {
        result.ops.emplace_back(api_operation_object(op));
        return true;
      }
    }
  );

  result.next_block_range_begin = next_values.first;
  result.next_operation_begin = next_values.second;

  return result;
}

} // detail

account_history_api::account_history_api()
{
  auto ah_cb = appbase::app().find_plugin< hive::plugins::account_history::account_history_plugin >();
  auto ah_rocks = appbase::app().find_plugin< hive::plugins::account_history_rocksdb::account_history_rocksdb_plugin >();

  if( ah_rocks != nullptr )
  {
    if( ah_cb != nullptr )
      wlog( "account_history and account_history_rocksdb plugins are both enabled. account_history_api will query from account_history_rocksdb" );

    my = std::make_unique< detail::account_history_api_rocksdb_impl >();
  }
  else if( ah_cb != nullptr )
  {
    my = std::make_unique< detail::account_history_api_chainbase_impl >();
  }
  else
  {
    FC_ASSERT( false, "Account History API only works if account_history or account_history_rocksdb plugins are enabled" );
  }

  JSON_RPC_REGISTER_API( HIVE_ACCOUNT_HISTORY_API_PLUGIN_NAME );
}

account_history_api::~account_history_api() {}

DEFINE_LOCKLESS_APIS( account_history_api ,
  (get_ops_in_block)
  (get_transaction)
  (get_account_history)
  (enum_virtual_ops)
)

} } } // hive::plugins::account_history
