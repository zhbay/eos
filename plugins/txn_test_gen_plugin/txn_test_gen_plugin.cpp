/**
 *  @file
 *  @copyright defined in eos/LICENSE.txt
 */
#include <eosio/txn_test_gen_plugin/txn_test_gen_plugin.hpp>
#include <eosio/chain_plugin/chain_plugin.hpp>
#include <eosio/chain/wast_to_wasm.hpp>
#include <eosio/utilities/key_conversion.hpp>

#include <fc/variant.hpp>
#include <fc/io/json.hpp>
#include <fc/exception/exception.hpp>
#include <fc/reflect/variant.hpp>
#include <fc/io/json.hpp>

#include <boost/asio/high_resolution_timer.hpp>
#include <boost/algorithm/clamp.hpp>

#include <Inline/BasicTypes.h>
#include <IR/Module.h>
#include <IR/Validate.h>
#include <WAST/WAST.h>
#include <WASM/WASM.h>
#include <Runtime/Runtime.h>

#include <eosio.token/eosio.token.wast.hpp>
#include <eosio.token/eosio.token.abi.hpp>

namespace eosio { namespace detail {
  struct txn_test_gen_empty {};
}}

FC_REFLECT(eosio::detail::txn_test_gen_empty, );

namespace eosio {

static appbase::abstract_plugin& _txn_test_gen_plugin = app().register_plugin<txn_test_gen_plugin>();

using namespace eosio::chain;

#define CALL(api_name, api_handle, call_name, INVOKE, http_response_code) \
{std::string("/v1/" #api_name "/" #call_name), \
   [this](string, string body, url_response_callback cb) mutable { \
          try { \
             if (body.empty()) body = "{}"; \
             INVOKE \
             cb(http_response_code, fc::json::to_string(result)); \
          } catch (...) { \
             http_plugin::handle_exception(#api_name, #call_name, body, cb); \
          } \
       }}

#define INVOKE_V_R_R_R(api_handle, call_name, in_param0, in_param1, in_param2) \
     const auto& vs = fc::json::json::from_string(body).as<fc::variants>(); \
     api_handle->call_name(vs.at(0).as<in_param0>(), vs.at(1).as<in_param1>(), vs.at(2).as<in_param2>()); \
     eosio::detail::txn_test_gen_empty result;

#define INVOKE_V_R_R(api_handle, call_name, in_param0, in_param1) \
     const auto& vs = fc::json::json::from_string(body).as<fc::variants>(); \
     api_handle->call_name(vs.at(0).as<in_param0>(), vs.at(1).as<in_param1>()); \
     eosio::detail::txn_test_gen_empty result;

#define INVOKE_V_V(api_handle, call_name) \
     api_handle->call_name(); \
     eosio::detail::txn_test_gen_empty result;

#define CALL_ASYNC(api_name, api_handle, call_name, INVOKE, http_response_code) \
{std::string("/v1/" #api_name "/" #call_name), \
   [this](string, string body, url_response_callback cb) mutable { \
      if (body.empty()) body = "{}"; \
      auto result_handler = [cb, body](const fc::exception_ptr& e) {\
         if (e) {\
            try {\
               e->dynamic_rethrow_exception();\
            } catch (...) {\
               http_plugin::handle_exception(#api_name, #call_name, body, cb);\
            }\
         } else {\
            cb(http_response_code, fc::json::to_string(eosio::detail::txn_test_gen_empty())); \
         }\
      };\
      INVOKE \
   }\
}

#define INVOKE_ASYNC_R_R_R(api_handle, call_name, in_param0, in_param1,in_param2) \
   const auto& vs = fc::json::json::from_string(body).as<fc::variants>(); \
   api_handle->call_name(vs.at(0).as<in_param0>(), vs.at(1).as<in_param1>(),vs.at(2).as<in_param2>(), result_handler);

struct txn_test_gen_plugin_impl {
   static void push_next_transaction(const std::shared_ptr<std::vector<signed_transaction>>& trxs, size_t index, const std::function<void(const fc::exception_ptr&)>& next ) {
     // ilog("push_next_transaction  index ${p} ", ("p", index));
      chain_plugin& cp = app().get_plugin<chain_plugin>();
      cp.accept_transaction( packed_transaction(trxs->at(index)), [=](const fc::static_variant<fc::exception_ptr, transaction_trace_ptr>& result){
         if (result.contains<fc::exception_ptr>()) {
            next(result.get<fc::exception_ptr>());
         } else {
            if (index + 1 < trxs->size()) {
               push_next_transaction(trxs, index + 1, next);
            } else {
               next(nullptr);
            }
         }
      });
//      if (index + 1 < trxs->size()) {
//         push_next_transaction(trxs, index + 1, next);
//      } else {
//         next(nullptr);
//      }
   }

   void push_transactions( std::vector<signed_transaction>&& trxs, const std::function<void(fc::exception_ptr)>& next ) {
      auto trxs_copy = std::make_shared<std::decay_t<decltype(trxs)>>(std::move(trxs));
      push_next_transaction(trxs_copy, 0, next);
   }

   void create_test_accounts(const std::string& init_name, const std::string& init_priv_key, uint64_t usercount,const std::function<void(const fc::exception_ptr&)>& next) {
      std::vector<signed_transaction> trxs;
      trxs.reserve(usercount*2+3);

      try {

         if(usercount<2)
             return ;
         if(accounts.size()>0)
             return ;
         name newaccountC("txn.test.t");
         name creator(init_name);

        // abi_def currency_abi_def = fc::json::from_string(eosio_token_abi).as<abi_def>();

         controller& cc = app().get_plugin<chain_plugin>().chain();
         auto chainid = app().get_plugin<chain_plugin>().get_chain_id();
         auto abi_serializer_max_time = app().get_plugin<chain_plugin>().get_abi_serializer_max_time();

         abi_serializer eosio_token_serializer{fc::json::from_string(eosio_token_abi).as<abi_def>(), abi_serializer_max_time};

//         fc::crypto::private_key txn_test_receiver_A_priv_key = fc::crypto::private_key::regenerate(fc::sha256(std::string(64, 'a')));
//         fc::crypto::private_key txn_test_receiver_B_priv_key = fc::crypto::private_key::regenerate(fc::sha256(std::string(64, 'b')));
//         fc::crypto::private_key txn_test_receiver_C_priv_key = fc::crypto::private_key::regenerate(fc::sha256(std::string(64, 'c')));
//         fc::crypto::public_key  txn_text_receiver_A_pub_key = txn_test_receiver_A_priv_key.get_public_key();
//         fc::crypto::public_key  txn_text_receiver_B_pub_key = txn_test_receiver_B_priv_key.get_public_key();
//         fc::crypto::public_key  txn_text_receiver_C_pub_key = txn_test_receiver_C_priv_key.get_public_key();
         fc::crypto::private_key creator_priv_key = fc::crypto::private_key(init_priv_key);
         fc::crypto::public_key creator_pub_key =  creator_priv_key.get_public_key();

         //create some test accounts
//         {
//            signed_transaction trx;

//            //create "A" account
//            {
//            auto owner_auth   = eosio::chain::authority{1, {{txn_text_receiver_A_pub_key, 1}}, {}};
//            auto active_auth  = eosio::chain::authority{1, {{txn_text_receiver_A_pub_key, 1}}, {}};

//            trx.actions.emplace_back(vector<chain::permission_level>{{creator,"active"}}, newaccount{creator, newaccountA, owner_auth, active_auth});
//            }
//            //create "B" account
//            {
//            auto owner_auth   = eosio::chain::authority{1, {{txn_text_receiver_B_pub_key, 1}}, {}};
//            auto active_auth  = eosio::chain::authority{1, {{txn_text_receiver_B_pub_key, 1}}, {}};

//            trx.actions.emplace_back(vector<chain::permission_level>{{creator,"active"}}, newaccount{creator, newaccountB, owner_auth, active_auth});
//            }
            //create "txn.test.t" account
            {
                signed_transaction trx;
                auto owner_auth   = eosio::chain::authority{1, {{creator_pub_key, 1}}, {}};
                auto active_auth  = eosio::chain::authority{1, {{creator_pub_key, 1}}, {}};

                trx.actions.emplace_back(vector<chain::permission_level>{{creator,"active"}}, newaccount{creator, newaccountC, owner_auth, active_auth});


                trx.expiration = cc.head_block_time() + fc::seconds(30);
                trx.set_reference_block(cc.head_block_id());
                trx.sign(creator_priv_key, chainid);
                trxs.emplace_back(std::move(trx));
            }


         string charmap = "abcdefghijklmnopqrstuvwxyz";

         accounts.reserve(usercount);
         uint64_t createcount=usercount;

         for(size_t a=0;a<charmap.size();a++)
             for(size_t b=0;b<charmap.size();b++)
                 for(size_t c=0;c<charmap.size();c++)
                     for(size_t d=0;d<charmap.size();d++)
                     {
                            signed_transaction trx;
                            string username=charmap.substr(a,1)+charmap.substr(b,1)+charmap.substr(c,1)+charmap.substr(d,1);
                            name user_account(username);
                            accounts.emplace_back(user_account);
                            auto owner_auth   = eosio::chain::authority{1, {{creator_pub_key, 1}}, {}};
                            auto active_auth  = eosio::chain::authority{1, {{creator_pub_key, 1}}, {}};

                            trx.actions.emplace_back(vector<chain::permission_level>{{creator,"active"}}, newaccount{creator, user_account, owner_auth, active_auth});

                             trx.expiration = cc.head_block_time() + fc::seconds(30);
                             trx.set_reference_block(cc.head_block_id());
                             trx.sign(creator_priv_key, chainid);
                             trxs.emplace_back(std::move(trx));

                             createcount--;
                             if(createcount==0)
                                 goto createfinished;
                     }

createfinished:




         //set txn.test.t contract to eosio.token & initialize it
         {
            signed_transaction trx;

            vector<uint8_t> wasm = wast_to_wasm(std::string(eosio_token_wast));

            setcode handler;
            handler.account = newaccountC;
            handler.code.assign(wasm.begin(), wasm.end());

            trx.actions.emplace_back( vector<chain::permission_level>{{newaccountC,"active"}}, handler);
            {
                setabi handler;
                handler.account = newaccountC;
                handler.abi = fc::raw::pack(json::from_string(eosio_token_abi).as<abi_def>());
                trx.actions.emplace_back( vector<chain::permission_level>{{newaccountC,"active"}}, handler);
            }
            trx.expiration = cc.head_block_time() + fc::seconds(30);
            trx.set_reference_block(cc.head_block_id());
            trx.sign(creator_priv_key, chainid);
            trxs.emplace_back(std::move(trx));

         }


        {
           signed_transaction trx;

           action act;
           act.account = N(txn.test.t);
           act.name = N(create);
           act.authorization = vector<permission_level>{{newaccountC,config::active_name}};
           act.data = eosio_token_serializer.variant_to_binary("create",
                     fc::json::from_string("{\"issuer\":\"txn.test.t\",\"maximum_supply\":\"100000000000.0000 CUR\"}"), abi_serializer_max_time);
           trx.actions.push_back(act);

         trx.expiration = cc.head_block_time() + fc::seconds(30);
         trx.set_reference_block(cc.head_block_id());
         trx.max_net_usage_words = 5000;
         trx.sign(creator_priv_key, chainid);
         trxs.emplace_back(std::move(trx));
        }


         for(auto iter:accounts)
         {
             signed_transaction trx;
            {
               action act;
               act.account = N(txn.test.t);
               act.name = N(issue);
               act.authorization = vector<permission_level>{{newaccountC,config::active_name}};

               act.data = eosio_token_serializer.variant_to_binary("issue",
                         fc::json::from_string(fc::format_string("{\"to\":\"${l}\",\"quantity\":\"1000000.0000 CUR\",\"memo\":\"\"}",
                         fc::mutable_variant_object()("l", iter.to_string().c_str()))), abi_serializer_max_time);
               trx.actions.push_back(act);
             }


            trx.expiration = cc.head_block_time() + fc::seconds(3600);
            trx.set_reference_block(cc.head_block_id());
            trx.max_net_usage_words = 5000;
            trx.sign(creator_priv_key, chainid);
            trxs.emplace_back(std::move(trx));

         }

      } catch (const fc::exception& e) {
         next(e.dynamic_copy_exception());
         return;
      }
      push_transactions(std::move(trxs), next);
   }

   void run()
   {
       while(running)
       {
           send_transaction([this](const fc::exception_ptr& e){
              if (e) {
                 elog("pushing transaction failed: ${e}", ("e", e->to_detail_string()));
                 //stop_generation();
              } else {
                 //arm_timer(timer.expires_at());
              }
           });
       }
   }

   void start_generation(const std::string& salt, const uint64_t& period, const uint64_t& batch_size) {
            if(running)
            {
                elog("pushing transaction stop");
                stop_generation();
                return ;
            }

            if(period < 1 || period > 2500)
               throw fc::exception(fc::invalid_operation_exception_code);
//            if(batch_size < 1 || batch_size > 250)
//               throw fc::exception(fc::invalid_operation_exception_code);
//            if(batch_size & 1)
//               throw fc::exception(fc::invalid_operation_exception_code);

            ilog("Started transaction test plugin; accounts ${p} transactions every ${m}ms", ("p", accounts.size())("m", period));
            if(accounts.size()<2)
                return ;
            running = true;

            controller& cc = app().get_plugin<chain_plugin>().chain();
            auto abi_serializer_max_time = app().get_plugin<chain_plugin>().get_abi_serializer_max_time();
            abi_serializer eosio_token_serializer{fc::json::from_string(eosio_token_abi).as<abi_def>(), abi_serializer_max_time};
            //create the actions here

            for(size_t acc=0;acc<accounts.size()-1; acc+=2)
            {
                action act;
                act.account = N(txn.test.t);
                act.name = N(transfer);
                act.authorization = vector<permission_level>{{accounts[acc],config::active_name}};
                string json=fc::format_string("{\"from\":\"${l}\",",
                                              fc::mutable_variant_object()("l", accounts[acc].to_string().c_str()));

                json+=fc::format_string("\"to\":\"${l}\",\"quantity\":\"1.0000 CUR\",\"memo\":\"\"}",
                                       fc::mutable_variant_object()("l", accounts[acc+1].to_string().c_str()));

                act.data = eosio_token_serializer.variant_to_binary("transfer",
                    fc::json::from_string(json),
                    abi_serializer_max_time);

                 test_actions.emplace_back(act);
            }
            ilog("create action  ${p}", ("p", test_actions.size()));

            timer_timeout = period;
            batch = batch_size;

            ilog("Started transaction test plugin; performing ${p} transactions every ${m}ms", ("p", batch_size)("m", period));

            arm_timer(boost::asio::high_resolution_timer::clock_type::now());

            int m_threadNum=1;


            for (int i=0;i<m_threadNum;i++)
            {
                //生成多个线程，绑定run函数，添加到线程组
                m_threadGroup.add_thread(
                    new boost::thread(boost::bind(&txn_test_gen_plugin_impl::run,this)));
            }


   }

   void arm_timer(boost::asio::high_resolution_timer::time_point s) {
      timer.expires_at(s + std::chrono::milliseconds(timer_timeout));
      timer.async_wait([this](const boost::system::error_code& ec) {
         if(!running || ec)
            return;

         send_transaction([this](const fc::exception_ptr& e){
            if (e) {
               elog("pushing transaction failed: ${e}", ("e", e->to_detail_string()));
               //stop_generation();
            } else {
               arm_timer(timer.expires_at());
            }
         });
      });
   }

   void send_transaction(std::function<void(const fc::exception_ptr&)> next) {
      std::vector<signed_transaction> trxs;
      trxs.reserve(batch);

      try {
         controller& cc = app().get_plugin<chain_plugin>().chain();
         auto chainid = app().get_plugin<chain_plugin>().get_chain_id();
         auto abi_serializer_max_time = app().get_plugin<chain_plugin>().get_abi_serializer_max_time();

         fc::crypto::private_key a_priv_key = fc::crypto::private_key::regenerate(fc::sha256(std::string(64, 'a')));
         fc::crypto::private_key b_priv_key = fc::crypto::private_key::regenerate(fc::sha256(std::string(64, 'b')));

         static uint64_t nonce = static_cast<uint64_t>(fc::time_point::now().sec_since_epoch()) << 32;
         abi_serializer eosio_serializer(cc.db().find<account_object, by_name>(config::system_account_name)->get_abi(), abi_serializer_max_time);

         uint32_t reference_block_num = cc.last_irreversible_block_num();
         if (txn_reference_block_lag >= 0) {
            reference_block_num = cc.head_block_num();
            if (reference_block_num <= (uint32_t)txn_reference_block_lag) {
               reference_block_num = 0;
            } else {
               reference_block_num -= (uint32_t)txn_reference_block_lag;
            }
         }

         block_id_type reference_block_id ;//= cc.get_block_id_for_num(reference_block_num);
         unsigned act_batch=0;
         unsigned act_index=0;
         while(true)
         {
             signed_transaction trx;
             trx.actions.push_back(test_actions[act_index]);
             trx.context_free_actions.emplace_back(action({}, config::null_account_name, "nonce", fc::raw::pack(nonce++)));
             //trx.actions.emplace_back(action({}, config::null_account_name, "nonce", fc::raw::pack(nonce++)));
             trx.set_reference_block(reference_block_id);
             trx.expiration = cc.head_block_time() + fc::seconds(3600);
             trx.max_net_usage_words = 100;
             trx.sign(a_priv_key, chainid);
             trxs.emplace_back(std::move(trx));
             act_index++;
             if(act_index>=test_actions.size())
                 act_index=0;

             act_batch++;
             if(act_batch>=batch)
                 break;


         }


      } catch ( const fc::exception& e ) {
         next(e.dynamic_copy_exception());
      }
      unsigned trans_count=trxs.size();
     // ilog("start push_transactions  ${p}", ("p", trans_count));
      push_transactions(std::move(trxs), next);
    //  ilog("finish push_transactions  ${p}", ("p", trans_count));
   }

   void stop_generation() {
      if(!running)
         throw fc::exception(fc::invalid_operation_exception_code);
      timer.cancel();
      running = false;
      test_actions.clear();
      m_threadGroup.join_all();
      ilog("Stopping transaction generation test");
   }

   boost::asio::high_resolution_timer timer{app().get_io_service()};
   bool running{false};

   unsigned timer_timeout;
   unsigned batch;


   std::vector<name> accounts;
   std::vector<action> test_actions;

   int32_t txn_reference_block_lag;
   boost::thread_group m_threadGroup;
};

txn_test_gen_plugin::txn_test_gen_plugin() {}
txn_test_gen_plugin::~txn_test_gen_plugin() {}

void txn_test_gen_plugin::set_program_options(options_description&, options_description& cfg) {
   cfg.add_options()
      ("txn-reference-block-lag", bpo::value<int32_t>()->default_value(0), "Lag in number of blocks from the head block when selecting the reference block for transactions (-1 means Last Irreversible Block)")
   ;
}

void txn_test_gen_plugin::plugin_initialize(const variables_map& options) {
   try {
      my.reset( new txn_test_gen_plugin_impl );
      my->txn_reference_block_lag = options.at( "txn-reference-block-lag" ).as<int32_t>();
   } FC_LOG_AND_RETHROW()
}

void txn_test_gen_plugin::plugin_startup() {
   app().get_plugin<http_plugin>().add_api({
      CALL_ASYNC(txn_test_gen, my, create_test_accounts, INVOKE_ASYNC_R_R_R(my, create_test_accounts, std::string, std::string,uint64_t), 200),
      CALL(txn_test_gen, my, stop_generation, INVOKE_V_V(my, stop_generation), 200),
      CALL(txn_test_gen, my, start_generation, INVOKE_V_R_R_R(my, start_generation, std::string, uint64_t, uint64_t), 200)
   });
}

void txn_test_gen_plugin::plugin_shutdown() {
   try {
      my->stop_generation();
   }
   catch(fc::exception e) {
   }
}

}
