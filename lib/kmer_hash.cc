//
// This file is part of khmer, http://github.com/ged-lab/khmer/, and is
// Copyright (C) Michigan State University, 2009-2013. It is licensed under
// the three-clause BSD license; see doc/LICENSE.txt.
// Contact: khmer-project@idyll.org
//

#include <math.h>
#include <string>
#include <iostream>
#include <algorithm>
#include <functional>
#include <string>
#include <gmpxx.h>

#include "khmer.hh"
#include "kmer_hash.hh"

using namespace std;

//
// _hash: hash a k-length DNA sequence into a 64-bit number.
//

namespace khmer
{

unsigned __int128 _hash(const char * kmer, const WordLength k,
                   unsigned __int128 &_h, unsigned __int128 &_r)
{
    // sizeof(HashIntoType) * 8 bits / 2 bits/base
    if (!(k <= sizeof(HashIntoType)*4) || !(strlen(kmer) >= k)) {
      cout << "Testing Different K-Size" << endl; 
    }

    unsigned __int128 h = 0, r = 0;

    h |= twobit_repr(kmer[0]);
    r |= twobit_comp(kmer[k-1]);

    for (WordLength i = 1, j = k - 2; i < k; i++, j--) {
        h = h << 2;
        r = r << 2;

        h |= twobit_repr(kmer[i]);
        r |= twobit_comp(kmer[j]);
    }

    _h = h;
    _r = r;

   // using a string of the lower valued kmer for hashing.
    hash<unsigned long long> kmer_ULL_hash;
    //string hashed = to_string(kmer_ULL_hash(result));
    unsigned long long hashed_h = kmer_ULL_hash(h);
    unsigned long long hashed_r = kmer_ULL_hash(r);
    cout << "TESTING C++ HASH: " << hashed << endl;
    // Hash the resulting lower kmer to the specified digest size
    
    return uniqify_rc(hashed_h, hashed_r);
}

// _hash: return the maximum of the forward and reverse hash.

HashIntoType _hash(const char * kmer, const WordLength k)
{
    HashIntoType h = 0;
    HashIntoType r = 0;

    return khmer::_hash(kmer, k, h, r);
}

// _hash_forward: return the hash from the forward direction only.

HashIntoType _hash_forward(const char * kmer, WordLength k)
{
    HashIntoType h = 0;
    HashIntoType r = 0;


    khmer::_hash(kmer, k, h, r);
    return h;			// return forward only
}

//
// _revhash: given an unsigned int, return the associated k-mer.
//

std::string _revhash(HashIntoType hash, WordLength k)
{
    std::string s = "";
    return s;
}


};
