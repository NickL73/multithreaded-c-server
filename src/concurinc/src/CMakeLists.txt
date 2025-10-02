add_library(coin STATIC coin.c)
target_link_libraries(coin PUBLIC pthread cards)