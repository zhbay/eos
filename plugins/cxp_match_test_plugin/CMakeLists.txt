file(GLOB HEADERS "include/eosio/cxp_match_test_plugin/*.hpp")
add_library( cxp_match_test_plugin
             cxp_match_test_plugin.cpp
             ${HEADERS} )

add_dependencies(cxp_match_test_plugin eosio.token)

target_link_libraries( cxp_match_test_plugin appbase fc http_plugin chain_plugin )
target_include_directories( cxp_match_test_plugin PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include" )
target_include_directories( cxp_match_test_plugin PUBLIC ${CMAKE_BINARY_DIR}/contracts )
