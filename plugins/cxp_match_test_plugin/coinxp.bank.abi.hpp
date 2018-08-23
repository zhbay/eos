const char* const coinxp_bank_abi = R"=====(
{
  "____comment": "This file was generated by eosio-abigen. DO NOT EDIT - 2018-08-21T10:14:45",
  "version": "eosio::abi/1.0",
  "types": [],
  "structs": [{
      "name": "account_asset",
      "base": "",
      "fields": [{
          "name": "balance",
          "type": "asset"
        },{
          "name": "locked",
          "type": "asset"
        }
      ]
    },{
      "name": "currency",
      "base": "",
      "fields": [{
          "name": "supply",
          "type": "asset"
        }
      ]
    },{
      "name": "deposit",
      "base": "",
      "fields": [{
          "name": "owner",
          "type": "name"
        },{
          "name": "quantity",
          "type": "asset"
        },{
          "name": "memo",
          "type": "string"
        }
      ]
    },{
      "name": "withdraw",
      "base": "",
      "fields": [{
          "name": "owner",
          "type": "name"
        },{
          "name": "quantity",
          "type": "asset"
        },{
          "name": "memo",
          "type": "string"
        }
      ]
    },{
      "name": "transfromtk",
      "base": "",
      "fields": [{
          "name": "owner",
          "type": "name"
        },{
          "name": "quantity",
          "type": "asset"
        },{
          "name": "memo",
          "type": "string"
        }
      ]
    },{
      "name": "transfromexc",
      "base": "",
      "fields": [{
          "name": "owner",
          "type": "name"
        },{
          "name": "exchange",
          "type": "name"
        },{
          "name": "quantity",
          "type": "asset"
        },{
          "name": "memo",
          "type": "string"
        }
      ]
    },{
      "name": "transtoexc",
      "base": "",
      "fields": [{
          "name": "owner",
          "type": "name"
        },{
          "name": "exchange",
          "type": "name"
        },{
          "name": "quantity",
          "type": "asset"
        },{
          "name": "memo",
          "type": "string"
        }
      ]
    },{
      "name": "createtoken",
      "base": "",
      "fields": [{
          "name": "token",
          "type": "asset"
        }
      ]
    },{
      "name": "lock",
      "base": "",
      "fields": [{
          "name": "owner",
          "type": "name"
        },{
          "name": "quantity",
          "type": "asset"
        }
      ]
    },{
      "name": "unlock",
      "base": "",
      "fields": [{
          "name": "owner",
          "type": "name"
        },{
          "name": "quantity",
          "type": "asset"
        }
      ]
    },{
      "name": "sublocked",
      "base": "",
      "fields": [{
          "name": "owner",
          "type": "name"
        },{
          "name": "quantity",
          "type": "asset"
        },{
          "name": "memo",
          "type": "string"
        }
      ]
    }
  ],
  "actions": [{
      "name": "deposit",
      "type": "deposit",
      "ricardian_contract": ""
    },{
      "name": "withdraw",
      "type": "withdraw",
      "ricardian_contract": ""
    },{
      "name": "transfromtk",
      "type": "transfromtk",
      "ricardian_contract": ""
    },{
      "name": "transfromexc",
      "type": "transfromexc",
      "ricardian_contract": ""
    },{
      "name": "transtoexc",
      "type": "transtoexc",
      "ricardian_contract": ""
    },{
      "name": "createtoken",
      "type": "createtoken",
      "ricardian_contract": ""
    },{
      "name": "lock",
      "type": "lock",
      "ricardian_contract": ""
    },{
      "name": "unlock",
      "type": "unlock",
      "ricardian_contract": ""
    },{
      "name": "sublocked",
      "type": "sublocked",
      "ricardian_contract": ""
    }
  ],
  "tables": [{
      "name": "accountasset",
      "index_type": "i64",
      "key_names": [
        "balance"
      ],
      "key_types": [
        "asset"
      ],
      "type": "account_asset"
    },{
      "name": "stat",
      "index_type": "i64",
      "key_names": [
        "supply"
      ],
      "key_types": [
        "asset"
      ],
      "type": "currency"
    }
  ],
  "ricardian_clauses": [],
  "error_messages": [],
  "abi_extensions": []
})=====";
