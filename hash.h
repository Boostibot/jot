#pragma once

#include <stdint.h>

namespace jot
{
    #ifndef JOT_SIZE_T
        using isize = ptrdiff_t;
    #endif

    uint64_t uint64_hash(uint64_t value) 
    {
        //source: https://stackoverflow.com/a/12996028
        uint64_t hash = value;
        hash = (hash ^ (hash >> 30)) * uint64_t(0xbf58476d1ce4e5b9);
        hash = (hash ^ (hash >> 27)) * uint64_t(0x94d049bb133111eb);
        hash = hash ^ (hash >> 31);
        return hash;
    }

    uint32_t uint32_hash(uint32_t value) 
    {
        //source: https://stackoverflow.com/a/12996028
        uint32_t hash = value;
        hash = ((hash >> 16) ^ hash) * 0x119de1f3;
        hash = ((hash >> 16) ^ hash) * 0x119de1f3;
        hash = (hash >> 16) ^ hash;
        return hash;
    }

    uint32_t murmur_hash32(const void* key, isize size, uint32_t seed)
    {
        //source https://github.com/abrandoned/murmur2/blob/master/MurmurHash2.c
        // 'm' and 'r' are mixing constants generated offline.
        // They're not really 'magic', they just happen to work well. 
        const uint32_t m = 0x5bd1e995;
        const int r = 24;

        // Initialize the hash to a 'random' value
        uint32_t h = seed ^ (uint32_t) size;

        // Mix 4 bytes at a time into the hash
        const uint8_t* data = (const uint8_t*)key;

        while(size >= 4)
        {
            uint32_t k = *(uint32_t*)data;

            k *= m;
            k ^= k >> r;
            k *= m;

            h *= m;
            h ^= k;

            data += 4;
            size -= 4;
        }

        // Handle the last few bytes of the input array 
        switch(size)
        {
            case 3: h ^= data[2] << 16; [[fallthrough]];
            case 2: h ^= data[1] << 8;  [[fallthrough]];
            case 1: h ^= data[0];       
            h *= m;
        };

        // Do a few final mixes of the hash to ensure the last few
        // bytes are well-incorporated. 

        h ^= h >> 13;
        h *= m;
        h ^= h >> 15;

        return h;
    } 

    uint64_t murmur_hash64(const void* key, isize size, uint64_t seed)
    {
        //source https://github.com/abrandoned/murmur2/blob/master/MurmurHash2.c
        //& big endian support: https://github.com/niklas-ourmachinery/bitsquid-foundation/blob/master/murmur_hash.cpp
        const uint64_t m = 0xc6a4a7935bd1e995;
        const int r = 47;

        uint64_t h = seed ^ (size * m);

        const uint64_t * data = (const uint64_t *)key;
        const uint64_t * end = data + (size/8);

        while(data != end)
        {
            #ifdef PLATFORM_BIG_ENDIAN
				uint64 k = *data++;
				char *p = (char *)&k;
				char c;
				c = p[0]; p[0] = p[7]; p[7] = c;
				c = p[1]; p[1] = p[6]; p[6] = c;
				c = p[2]; p[2] = p[5]; p[5] = c;
				c = p[3]; p[3] = p[4]; p[4] = c;
			#else
				uint64_t k = *data++;
			#endif

            k *= m; 
            k ^= k >> r; 
            k *= m; 
    
            h ^= k;
            h *= m; 
        }
    
        const uint8_t* data2 = (const uint8_t*)key;
        switch(size & 7)
        {
            case 7: h ^= ((uint64_t) data2[6]) << 48; [[fallthrough]];
            case 6: h ^= ((uint64_t) data2[5]) << 40; [[fallthrough]];
            case 5: h ^= ((uint64_t) data2[4]) << 32; [[fallthrough]];
            case 4: h ^= ((uint64_t) data2[3]) << 24; [[fallthrough]];
            case 3: h ^= ((uint64_t) data2[2]) << 16; [[fallthrough]];
            case 2: h ^= ((uint64_t) data2[1]) << 8;  [[fallthrough]];
            case 1: h ^= ((uint64_t) data2[0]);       
            h *= m;
        };
 
        h ^= h >> r;
        h *= m;
        h ^= h >> r;

        return h;
    } 

    uint32_t fnv_one_at_a_time_hash32(const void* key, isize size, uint32_t seed)
    {
        // Source: https://github.com/aappleby/smhasher/blob/master/src/Hashes.cpp
        uint32_t hash = seed;
        hash ^= 2166136261UL;
        const uint8_t* data = (const uint8_t*)key;
        for(isize i = 0; i < size; i++)
        {
            hash ^= data[i];
            hash *= 16777619;
        }
        return hash;
    }

    uint32_t murmur_one_at_a_time_hash32(const void* key, isize size, uint32_t seed)
    {
        // One-byte-at-a-time hash based on Murmur's mix
        // Source: https://github.com/aappleby/smhasher/blob/master/src/Hashes.cpp
        uint32_t hash = seed;
        const uint8_t* data = (const uint8_t*)key;
        for(isize i = 0; i < size; i++)
        {
            hash ^= data[i];
            hash *= 0x5bd1e995;
            hash ^= hash >> 15;
        }
        return hash;
    }

    uint32_t jenkins_one_at_a_time_hash32(const void* key, isize size, uint32_t seed)
    {
        uint32_t hash = seed;
        const uint8_t* data = (const uint8_t*)key;
        for(isize i = 0; i < size; i++)
        {
            hash += data[i];
            hash += (hash << 10);
            hash ^= (hash >> 6);
        }
        hash += (hash << 3);
        hash ^= (hash >> 11);
        hash += (hash << 15);
        return hash;
    }

    uint32_t rotl32(uint32_t value, int32_t by_bits)
    {
        // C idiom: will be optimized to a single operation
        return value<<by_bits | value>>(32-by_bits);      
    }

    uint32_t coffin_one_at_a_time_hash_32(const void* key, isize size, uint32_t seed) 
    { 
        // Source: https://stackoverflow.com/a/7666668/5407270
        uint32_t hash = seed ^ 0x55555555;
        const uint8_t* data = (const uint8_t*)key;
        for(isize i = 0; i < size; i++)
        {
            hash ^= data[i];
            hash = rotl32(hash, 5);
        }
        return hash;
    }
}