/* date = August 26th 2024 21:13 */
#pragma once
#include <unordered_map>
#include <list>
#include <stdint.h>
#include <mutex>
#include <optional>
#include <functional>
#include <string>
#include <iostream>

#if defined(LRU_CACHE_DEBUG)
    #define lru_print(...) std::cout << __VA_ARGS__ << std::endl;
#else
    #define lru_print(...)
#endif

template<typename KeyType, typename ItemType>
class LRUCache{
    public:

    LRUCache(size_t size, std::function<void(ItemType)> cln) :
        maxSize(size), cleanup(cln){}

    std::optional<ItemType> get(KeyType key){
        std::lock_guard<std::mutex> lock(cache_mutex);
        auto it = cache.find(key);
        if(it == cache.end()){
            lru_print("[LRU] Could not find key '" << printKey(key) << "'");
            return {};
        }

        order.erase(it->second.second);
        order.push_front(key);
        lru_print("[LRU] Pushing key '" << printKey(key) << "' to front");
        lru_print("[LRU] Returning '" << printItem(it->first) << "'");
        it->second.second = order.begin();
        return std::optional<ItemType>(it->first);
    }

    void put(KeyType key, ItemType item){
        std::lock_guard<std::mutex> lock(cache_mutex);
        if(cache.size() >= maxSize){
            KeyType lru = order.back();
            order.pop_back();

            lru_print("[LRU] Max Size, clearing '" << printKey(lru) << "'");
            auto it = cache.find(lru);
            cleanup(it->second.first);
            cache.erase(lru);
        }

        lru_print("[LRU] Putting key '" << printKey(key) << "' into cache");
        order.push_front(key);
        cache.insert(std::make_pair(key, std::make_pair(item, order.begin())));
    }

#if defined(LRU_CACHE_DEBUG)
    void set_dbg_functions(std::function<std::string(ItemType)> itemPrnt,
                           std::function<std::string(KeyType)> keyPrnt)
    {
        printItem = itemPrnt;
        printKey = keyPrnt;
    }

    void dbg_print_state(){
        std::cout << "------ CACHE ------" << std::endl;
        for(auto it : cache){
            std::cout << printKey(it.first) << " : " << printItem(it.second.first) << std::endl;
        }

        std::cout << "------ ORDER ------" << std::endl;
        for(auto it : order){
            std::cout << printKey(it) << std::endl;
        }

        std::cout << "-------------------" << std::endl;
    }

    std::function<std::string(ItemType)> printItem;
    std::function<std::string(KeyType)> printKey;
#else
    void dbg_print_state(){}
    void set_dbg_functions(std::function<std::string(ItemType)>,
                           std::function<std::string(KeyType)>){}

#endif

    size_t maxSize;
    std::mutex cache_mutex;
    std::unordered_map<KeyType, std::pair<ItemType,
                                typename std::list<KeyType>::iterator>> cache;
    std::list<KeyType> order;
    std::function<void(ItemType)> cleanup;
};
