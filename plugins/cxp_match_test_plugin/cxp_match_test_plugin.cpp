/**
 *  @file
 *  @copyright defined in eos/LICENSE.txt
 */
#include <eosio/cxp_match_test_plugin/cxp_match_test_plugin.hpp>
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

#include "./coinxp.match.abi.hpp"
#include "./coinxp.match.wast.hpp"
#include "./coinxp.latestprice.abi.hpp"
#include "./coinxp.latestprice.wast.hpp"
#include "./coinxp.orderdb.abi.hpp"
#include "./coinxp.orderdb.wast.hpp"
#include "./coinxp.exchange.abi.hpp"
#include "./coinxp.exchange.wast.hpp"
#include "./coinxp.bank.abi.hpp"
#include "./coinxp.bank.wast.hpp"

namespace eosio { namespace detail {
  struct cxp_match_test_empty {};
}}

FC_REFLECT(eosio::detail::cxp_match_test_empty, );

namespace eosio {

static appbase::abstract_plugin& _cxp_match_test_plugin = app().register_plugin<cxp_match_test_plugin>();

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
     eosio::detail::cxp_match_test_empty result;

#define INVOKE_V_R_R(api_handle, call_name, in_param0, in_param1) \
     const auto& vs = fc::json::json::from_string(body).as<fc::variants>(); \
     api_handle->call_name(vs.at(0).as<in_param0>(), vs.at(1).as<in_param1>()); \
     eosio::detail::txn_test_gen_empty result;

#define INVOKE_V_V(api_handle, call_name) \
     api_handle->call_name(); \
     eosio::detail::cxp_match_test_empty result;

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
            cb(http_response_code, fc::json::to_string(eosio::detail::cxp_match_test_empty())); \
         }\
      };\
      INVOKE \
   }\
}

#define INVOKE_ASYNC_R_R(api_handle, call_name, in_param0, in_param1) \
   const auto& vs = fc::json::json::from_string(body).as<fc::variants>(); \
   api_handle->call_name(vs.at(0).as<in_param0>(), vs.at(1).as<in_param1>(), result_handler);

struct cxp_match_test_plugin_impl {
   static void push_next_transaction(const std::shared_ptr<std::vector<signed_transaction>>& trxs, size_t index, const std::function<void(const fc::exception_ptr&)>& next ) {
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
   }

   void push_transactions( std::vector<signed_transaction>&& trxs, const std::function<void(fc::exception_ptr)>& next ) {
      auto trxs_copy = std::make_shared<std::decay_t<decltype(trxs)>>(std::move(trxs));
      push_next_transaction(trxs_copy, 0, next);
   }

   void create_test_accounts(const std::string& init_name, const std::string& init_priv_key, const std::function<void(const fc::exception_ptr&)>& next) {
      std::vector<signed_transaction> trxs;
      trxs.reserve(3);
      requestid=0;
      try {
         name account_usera("cxp.usera");
         name account_userb("cxp.userb");
         name account_exchange1("exchange1");
         name account_exchange2("exchange2");
         name account_cxp_match("cxp.match");
         name account_cxp_odb("cxp.odb");
         name account_cxp_lp("cxp.lp");
         name account_coinxp_bank("coinxp.bank");


         name creator(init_name);

         //abi_def currency_abi_def = fc::json::from_string(coinxp_match_abi).as<abi_def>();

         controller& cc = app().get_plugin<chain_plugin>().chain();
         auto chainid = app().get_plugin<chain_plugin>().get_chain_id();
         auto abi_serializer_max_time = app().get_plugin<chain_plugin>().get_abi_serializer_max_time();

         abi_serializer coinxp_match_serializer{fc::json::from_string(coinxp_match_abi).as<abi_def>(), abi_serializer_max_time};
         abi_serializer coinxp_bank_serializer{fc::json::from_string(coinxp_bank_abi).as<abi_def>(), abi_serializer_max_time};
         abi_serializer coinxp_exchange_serializer{fc::json::from_string(coinxp_exchange_abi).as<abi_def>(), abi_serializer_max_time};

         fc::crypto::private_key txn_test_receiver_A_priv_key = fc::crypto::private_key::regenerate(fc::sha256(std::string(64, 'a')));
//         fc::crypto::private_key txn_test_receiver_B_priv_key = fc::crypto::private_key::regenerate(fc::sha256(std::string(64, 'b')));
//         fc::crypto::private_key txn_test_receiver_C_priv_key = fc::crypto::private_key::regenerate(fc::sha256(std::string(64, 'c')));
//         fc::crypto::private_key txn_test_receiver_D_priv_key = fc::crypto::private_key::regenerate(fc::sha256(std::string(64, 'd')));
//         fc::crypto::private_key txn_test_receiver_E_priv_key = fc::crypto::private_key::regenerate(fc::sha256(std::string(64, 'e')));
//         fc::crypto::private_key txn_test_receiver_F_priv_key = fc::crypto::private_key::regenerate(fc::sha256(std::string(64, 'f')));

         fc::crypto::public_key  txn_text_receiver_A_pub_key = txn_test_receiver_A_priv_key.get_public_key();
//         fc::crypto::public_key  txn_text_receiver_B_pub_key = txn_test_receiver_B_priv_key.get_public_key();
//         fc::crypto::public_key  txn_text_receiver_C_pub_key = txn_test_receiver_C_priv_key.get_public_key();
//         fc::crypto::public_key  txn_text_receiver_D_pub_key = txn_test_receiver_D_priv_key.get_public_key();
//         fc::crypto::public_key  txn_text_receiver_E_pub_key = txn_test_receiver_E_priv_key.get_public_key();
//         fc::crypto::public_key  txn_text_receiver_F_pub_key = txn_test_receiver_F_priv_key.get_public_key();
         fc::crypto::private_key creator_priv_key = fc::crypto::private_key(init_priv_key);

         //create some test accounts
         {
            signed_transaction trx;

            //create "A" account
            {
            auto owner_auth   = eosio::chain::authority{1, {{txn_text_receiver_A_pub_key, 1}}, {}};
            auto active_auth  = eosio::chain::authority{1, {{txn_text_receiver_A_pub_key, 1}}, {}};

            trx.actions.emplace_back(vector<chain::permission_level>{{creator,"active"}}, newaccount{creator, account_usera, owner_auth, active_auth});
            }
            //create "B" account
            {
            auto owner_auth   = eosio::chain::authority{1, {{txn_text_receiver_A_pub_key, 1}}, {}};
            auto active_auth  = eosio::chain::authority{1, {{txn_text_receiver_A_pub_key, 1}}, {}};

            trx.actions.emplace_back(vector<chain::permission_level>{{creator,"active"}}, newaccount{creator, account_userb, owner_auth, active_auth});
            }
            //create "exchange1" account
            {
            auto owner_auth   = eosio::chain::authority{1, {{txn_text_receiver_A_pub_key, 1}}, {}};
            auto active_auth  = eosio::chain::authority{1, {{txn_text_receiver_A_pub_key, 1}}, {}};

            trx.actions.emplace_back(vector<chain::permission_level>{{creator,"active"}}, newaccount{creator, account_exchange1, owner_auth, active_auth});
            }
            //create "exchange2" account
            {
            auto owner_auth   = eosio::chain::authority{1, {{txn_text_receiver_A_pub_key, 1}}, {}};
            auto active_auth  = eosio::chain::authority{1, {{txn_text_receiver_A_pub_key, 1}}, {}};

            trx.actions.emplace_back(vector<chain::permission_level>{{creator,"active"}}, newaccount{creator, account_exchange2, owner_auth, active_auth});
            }
            //create "cxp.match" account
            {
            auto owner_auth   = eosio::chain::authority{1, {{txn_text_receiver_A_pub_key, 1}}, {}};
            auto active_auth  = eosio::chain::authority{1, {{txn_text_receiver_A_pub_key, 1}}, {}};

            trx.actions.emplace_back(vector<chain::permission_level>{{creator,"active"}}, newaccount{creator, account_cxp_match, owner_auth, active_auth});
            }
            //create "cxp.odb" account
            {
            auto owner_auth   = eosio::chain::authority{1, {{txn_text_receiver_A_pub_key, 1}}, {}};
            auto active_auth  = eosio::chain::authority{1, {{txn_text_receiver_A_pub_key, 1}}, {}};

            trx.actions.emplace_back(vector<chain::permission_level>{{creator,"active"}}, newaccount{creator, account_cxp_odb, owner_auth, active_auth});
            }
            //create "cxp.lp" account
            {
            auto owner_auth   = eosio::chain::authority{1, {{txn_text_receiver_A_pub_key, 1}}, {}};
            auto active_auth  = eosio::chain::authority{1, {{txn_text_receiver_A_pub_key, 1}}, {}};

            trx.actions.emplace_back(vector<chain::permission_level>{{creator,"active"}}, newaccount{creator, account_cxp_lp, owner_auth, active_auth});
            }

            //create "coinxp.bank" account
            {
            auto owner_auth   = eosio::chain::authority{1, {{txn_text_receiver_A_pub_key, 1}}, {}};
            auto active_auth  = eosio::chain::authority{1, {{txn_text_receiver_A_pub_key, 1}}, {}};

            trx.actions.emplace_back(vector<chain::permission_level>{{creator,"active"}}, newaccount{creator, account_coinxp_bank, owner_auth, active_auth});
            }


            trx.expiration = cc.head_block_time() + fc::seconds(30);
            trx.set_reference_block(cc.head_block_id());
            trx.sign(creator_priv_key, chainid);
            trxs.emplace_back(std::move(trx));
         }

         //set cxp.match contract  & initialize it
         {
            signed_transaction trx;

            vector<uint8_t> wasm = wast_to_wasm(std::string(coinxp_match_wast));
            setcode handler;
            handler.account = account_cxp_match;
            handler.code.assign(wasm.begin(), wasm.end());
            trx.actions.emplace_back( vector<chain::permission_level>{{account_cxp_match,"active"}}, handler);

            wasm = wast_to_wasm(std::string(coinxp_orderdb_wast));
            handler.account = account_cxp_odb;
            handler.code.assign(wasm.begin(), wasm.end());
            trx.actions.emplace_back( vector<chain::permission_level>{{account_cxp_odb,"active"}}, handler);

            wasm = wast_to_wasm(std::string(coinxp_latestprice_wast));
            handler.account = account_cxp_lp;
            handler.code.assign(wasm.begin(), wasm.end());
            trx.actions.emplace_back( vector<chain::permission_level>{{account_cxp_lp,"active"}}, handler);

            wasm = wast_to_wasm(std::string(coinxp_exchange_wast));
            handler.account = account_exchange1;
            handler.code.assign(wasm.begin(), wasm.end());
            trx.actions.emplace_back( vector<chain::permission_level>{{account_exchange1,"active"}}, handler);

            wasm = wast_to_wasm(std::string(coinxp_exchange_wast));
            handler.account = account_exchange2;
            handler.code.assign(wasm.begin(), wasm.end());
            trx.actions.emplace_back( vector<chain::permission_level>{{account_exchange2,"active"}}, handler);

            wasm = wast_to_wasm(std::string(coinxp_bank_wast));
            handler.account = account_coinxp_bank;
            handler.code.assign(wasm.begin(), wasm.end());
            trx.actions.emplace_back( vector<chain::permission_level>{{account_coinxp_bank,"active"}}, handler);



            {
               setabi handler;
               handler.account = account_cxp_match;
               handler.abi = fc::raw::pack(json::from_string(coinxp_match_abi).as<abi_def>());
               trx.actions.emplace_back( vector<chain::permission_level>{{account_cxp_match,"active"}}, handler);

               handler.account = account_cxp_odb;
               handler.abi = fc::raw::pack(json::from_string(coinxp_orderdb_abi).as<abi_def>());
               trx.actions.emplace_back( vector<chain::permission_level>{{account_cxp_odb,"active"}}, handler);

               handler.account = account_cxp_lp;
               handler.abi = fc::raw::pack(json::from_string(coinxp_latestprice_abi).as<abi_def>());
               trx.actions.emplace_back( vector<chain::permission_level>{{account_cxp_lp,"active"}}, handler);

               handler.account = account_exchange1;
               handler.abi = fc::raw::pack(json::from_string(coinxp_exchange_abi).as<abi_def>());
               trx.actions.emplace_back( vector<chain::permission_level>{{account_exchange1,"active"}}, handler);

               handler.account = account_exchange2;
               handler.abi = fc::raw::pack(json::from_string(coinxp_exchange_abi).as<abi_def>());
               trx.actions.emplace_back( vector<chain::permission_level>{{account_exchange2,"active"}}, handler);

               handler.account = account_coinxp_bank;
               handler.abi = fc::raw::pack(json::from_string(coinxp_bank_abi).as<abi_def>());
               trx.actions.emplace_back( vector<chain::permission_level>{{account_coinxp_bank,"active"}}, handler);
            }



            {
               action act;
               act.account = N(coinxp.bank);
               act.name = N(createtoken);
               act.authorization = vector<permission_level>{{account_coinxp_bank,config::active_name}};
               act.data = coinxp_bank_serializer.variant_to_binary("createtoken", fc::json::from_string("{\"token\":\"1.00000000 BTC\"}"), abi_serializer_max_time);
               trx.actions.push_back(act);
            }


            {
               action act;
               act.account = N(coinxp.bank);
               act.name = N(createtoken);
               act.authorization = vector<permission_level>{{account_coinxp_bank,config::active_name}};
               act.data = coinxp_bank_serializer.variant_to_binary("createtoken", fc::json::from_string("{\"token\":\"1.00000000 ETH\"}"), abi_serializer_max_time);
               trx.actions.push_back(act);
            }
            {
               action act;
               act.account = N(cxp.match);
               act.name = N(addpair);
               act.authorization = vector<permission_level>{{account_cxp_match,config::active_name}};
               act.data = coinxp_match_serializer.variant_to_binary("addpair", fc::json::from_string("{\"currency\":\"1.00000000 BTC\",\"commodity\":\"1.00000000 ETH\"}"), abi_serializer_max_time);
               trx.actions.push_back(act);
            }

            {
               action act;
               act.account = N(exchange1);
               act.name = N(addbalance);
               act.authorization = vector<permission_level>{{account_exchange1,config::active_name}};
               act.data = coinxp_exchange_serializer.variant_to_binary("addbalance", fc::json::from_string("{\"owner\":\"cxp.usera\",\"value\":\"46116860184.27387903 BTC\"}"), abi_serializer_max_time);
               trx.actions.push_back(act);
            }

            {
               action act;
               act.account = N(exchange2);
               act.name = N(addbalance);
               act.authorization = vector<permission_level>{{account_exchange2,config::active_name}};
               act.data = coinxp_exchange_serializer.variant_to_binary("addbalance", fc::json::from_string("{\"owner\":\"cxp.userb\",\"value\":\"46116860184.27387903 ETH\"}"), abi_serializer_max_time);
               trx.actions.push_back(act);
            }


            trx.expiration = cc.head_block_time() + fc::seconds(30);
            trx.set_reference_block(cc.head_block_id());
            trx.max_net_usage_words = 5000;
            trx.sign(txn_test_receiver_A_priv_key, chainid);
            trxs.emplace_back(std::move(trx));
         }



      } catch (const fc::exception& e) {
         next(e.dynamic_copy_exception());
         return;
      }

      push_transactions(std::move(trxs), next);
   }

   void start_generation(const std::string& salt, const uint64_t& period, const uint64_t& batch_size) {
      if(running)
         throw fc::exception(fc::invalid_operation_exception_code);
      if(period < 1 || period > 2500)
         throw fc::exception(fc::invalid_operation_exception_code);
      if(batch_size < 1 || batch_size > 250)
         throw fc::exception(fc::invalid_operation_exception_code);
      if(batch_size & 1)
         throw fc::exception(fc::invalid_operation_exception_code);

      if(running)
          stop_generation();

      running = true;
      controller& cc = app().get_plugin<chain_plugin>().chain();
      auto abi_serializer_max_time = app().get_plugin<chain_plugin>().get_abi_serializer_max_time();
      abi_serializer coinxp_match_serializer{fc::json::from_string(coinxp_match_abi).as<abi_def>(), abi_serializer_max_time};




      //create the actions here
      act_ask_buy.account = N(cxp.match);
      act_ask_buy.name = N(ask);
      act_ask_buy.authorization = vector<permission_level>{{name("cxp.usera"),config::active_name}};
      //act_ask_buy.data = coinxp_match_serializer.variant_to_binary("ask", fc::json::from_string("{\"user\":\"cxp.usera\",\"exchange\":\"exchange\",\"currency\":\"100.00000000 BTC\",\"commodity\":\"400.00000000 ETH\",\"is_buy\":true,\"request_id\":1}"),abi_serializer_max_time);

      act_ask_sell.account = N(cxp.match);
      act_ask_sell.name = N(ask);
      act_ask_sell.authorization = vector<permission_level>{{name("cxp.userb"),config::active_name}};
      //act_ask_sell.data = coinxp_match_serializer.variant_to_binary("ask", fc::json::from_string("{\"user\":\"cxp.userb\",\"exchange\":\"exchange\",\"currency\":\"100.00000000 BTC\",\"commodity\":\"400.00000000 ETH\",\"is_buy\":false,\"request_id\":2}"),abi_serializer_max_time);
      timer_timeout = period;
      batch = batch_size/2;

      ilog("Started transaction test plugin; performing ${p} transactions every ${m}ms", ("p", batch_size)("m", period));

      arm_timer(boost::asio::high_resolution_timer::clock_type::now());
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
      trxs.reserve(2*batch);

      try {
         controller& cc = app().get_plugin<chain_plugin>().chain();
         auto chainid = app().get_plugin<chain_plugin>().get_chain_id();
         auto abi_serializer_max_time = app().get_plugin<chain_plugin>().get_abi_serializer_max_time();

         fc::crypto::private_key a_priv_key = fc::crypto::private_key::regenerate(fc::sha256(std::string(64, 'a')));
         fc::crypto::private_key b_priv_key = fc::crypto::private_key::regenerate(fc::sha256(std::string(64, 'b')));

         static uint64_t nonce = static_cast<uint64_t>(fc::time_point::now().sec_since_epoch()) << 32;
         //abi_serializer coinxp_match_serializer(cc.db().find<account_object, by_name>(config::system_account_name)->get_abi(), abi_serializer_max_time);
         abi_serializer coinxp_match_serializer{fc::json::from_string(coinxp_match_abi).as<abi_def>(), abi_serializer_max_time};

         uint32_t reference_block_num = cc.last_irreversible_block_num();
         if (txn_reference_block_lag >= 0) {
            reference_block_num = cc.head_block_num();
            if (reference_block_num <= (uint32_t)txn_reference_block_lag) {
               reference_block_num = 0;
            } else {
               reference_block_num -= (uint32_t)txn_reference_block_lag;
            }
         }

         block_id_type reference_block_id = cc.get_block_id_for_num(reference_block_num);


//         act_b_to_a.data = eosio_token_serializer.variant_to_binary("transfer",
//                                                                     fc::json::from_string(fc::format_string("{\"from\":\"txn.test.b\",\"to\":\"txn.test.a\",\"quantity\":\"1.0000 CUR\",\"memo\":\"${l}\"}",
//                                                                     fc::mutable_variant_object()("l", salt))),
//                                                                     abi_serializer_max_time);
         for(unsigned int i = 0; i < batch; ++i) {
             {
                 requestid++;
                 signed_transaction trx;
                 act_ask_buy.data = coinxp_match_serializer.variant_to_binary("ask",
                    fc::json::from_string(fc::format_string("{\"user\":\"cxp.usera\",\"exchange\":\"exchange1\",\"currency\":\"10.00000000 BTC\",\"commodity\":\"40.00000000 ETH\",\"is_buy\":true,\"request_id\":${l}}",
                    fc::mutable_variant_object()("l", requestid))),abi_serializer_max_time);
                 trx.actions.push_back(act_ask_buy);
                 trx.context_free_actions.emplace_back(action({}, config::null_account_name, "nonce", fc::raw::pack(nonce++)));
                 trx.set_reference_block(reference_block_id);
                 trx.expiration = cc.head_block_time() + fc::seconds(30);
                 trx.max_net_usage_words = 100;
                 trx.sign(a_priv_key, chainid);
                 trxs.emplace_back(std::move(trx));
             }

             {
                 requestid++;
                 signed_transaction trx;
                 act_ask_sell.data = coinxp_match_serializer.variant_to_binary("ask",
                    fc::json::from_string(fc::format_string("{\"user\":\"cxp.userb\",\"exchange\":\"exchange2\",\"currency\":\"10.00000000 BTC\",\"commodity\":\"40.00000000 ETH\",\"is_buy\":false,\"request_id\":${l}}",
                    fc::mutable_variant_object()("l", requestid))),abi_serializer_max_time);
                 trx.actions.push_back(act_ask_sell);
                 trx.context_free_actions.emplace_back(action({}, config::null_account_name, "nonce", fc::raw::pack(nonce++)));
                 trx.set_reference_block(reference_block_id);
                 trx.expiration = cc.head_block_time() + fc::seconds(30);
                 trx.max_net_usage_words = 100;
                 trx.sign(b_priv_key, chainid);
                 trxs.emplace_back(std::move(trx));
             }
         }
      } catch ( const fc::exception& e ) {
         next(e.dynamic_copy_exception());
      }

      push_transactions(std::move(trxs), next);
   }

   void stop_generation() {
      if(!running)
         throw fc::exception(fc::invalid_operation_exception_code);
      timer.cancel();
      running = false;
      ilog("Stopping transaction generation test");
   }

   boost::asio::high_resolution_timer timer{app().get_io_service()};
   bool running{false};

   unsigned timer_timeout;
   unsigned batch;

   action act_ask_buy;
   action act_ask_sell;
   uint64_t requestid;
   int32_t txn_reference_block_lag;
};

cxp_match_test_plugin::cxp_match_test_plugin() {}
cxp_match_test_plugin::~cxp_match_test_plugin() {}

void cxp_match_test_plugin::set_program_options(options_description&, options_description& cfg) {
   cfg.add_options()
      ("cxp_match-reference-block-lag", bpo::value<int32_t>()->default_value(0), "Lag in number of blocks from the head block when selecting the reference block for transactions (-1 means Last Irreversible Block)")
   ;
}

void cxp_match_test_plugin::plugin_initialize(const variables_map& options) {
   try {
      my.reset( new cxp_match_test_plugin_impl );
      my->txn_reference_block_lag = options.at( "cxp_match-reference-block-lag" ).as<int32_t>();
   } FC_LOG_AND_RETHROW()
}

void cxp_match_test_plugin::plugin_startup() {
   app().get_plugin<http_plugin>().add_api({
      CALL_ASYNC(cxp_match_test, my, create_test_accounts, INVOKE_ASYNC_R_R(my, create_test_accounts, std::string, std::string), 200),
      CALL(cxp_match_test, my, stop_generation, INVOKE_V_V(my, stop_generation), 200),
      CALL(cxp_match_test, my, start_generation, INVOKE_V_R_R_R(my, start_generation, std::string, uint64_t, uint64_t), 200)
   });

}

void cxp_match_test_plugin::plugin_shutdown() {
   try {
      my->stop_generation();
   }
   catch(fc::exception e) {
   }
}

}
