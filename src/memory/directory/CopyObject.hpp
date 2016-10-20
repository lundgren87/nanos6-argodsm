#ifndef CACHE_OBJECT_HPP
#define CACHE_OBJECT_HPP

#include "dependencies/linear-regions/DataAccessRange.hpp"

#include <boost/intrusive/avl_set.hpp>
#include "memory/cache/GenericCache.hpp"


class CopyObject {
private: 
	DataAccessRange _range;
	unsigned int _version;
	std::set<GenericCache *> _caches;

public:
	
	#if NDEBUG
		typedef boost::intrusive::avl_set_member_hook<boost::intrusive::link_mode<boost::intrusive::normal_link>> member_hook_t;
	#else
		typedef boost::intrusive::avl_set_member_hook<boost::intrusive::link_mode<boost::intrusive::safe_link>> member_hook_t;
	#endif	

	member_hook_t _hook;

	CopyObject(void *startAddress, size_t size);
	void *getStartAddress();
	size_t getSize();
	int getVersion();
	void setVerstion(int version);
	void incrementVersion();
	void addCache(GenericCache *cache);
	void removeCache(GenericCache *cache);
	bool isInCache(GenericCache *cache);
	int countCaches();

	/* Key for Boost Intrusive AVL Set */
    struct key_value
    {
        typedef void *type;

        const type &operator()(const CopyObject &obj){
            return obj._range.getStartAddress();
        }
    };

    friend key_value;

};

#endif //CACHE_OBJECT_HPP
