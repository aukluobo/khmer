#include "hashtable.hh"
#include "hashbits.hh"
#include "parsers.hh"

#define IO_BUF_SIZE 50*1000*1000
#define CALLBACK_PERIOD 10000
#define PARTITION_ALL_TAG_DEPTH 500

using namespace std;
using namespace khmer;

#if 1
HashIntoType khmer::primes[] = { 22906493,
				 22906519,
				 22906561,
				 22906567,
				 22906619,
				 22906649,
				 22906657,
				 22906661 };
#endif

#if 0
HashIntoType khmer::primes[] = { 32000000017,
				 32000000023,
				 32000000059,
				 32000000093,
				 32000000113,
				 32000000177,
				 32000000213,
				 32000000237 };
#endif

#if 0
HashIntoType khmer::primes[] = { 16000000039,
				 16000000067,
				 16000000091,
				 16000000097,
				 16000000103,
				 16000000127,
				 16000000157,
				 16000000201 };
#endif

//

void Hashbits::save(std::string outfilename)
{
  assert(_counts[0]);

  unsigned int save_ksize = _ksize;
  unsigned long long save_tablesize = _tablesize;

  ofstream outfile(outfilename.c_str(), ios::binary);

  outfile.write((const char *) &save_ksize, sizeof(save_ksize));

  for (unsigned int i = 0; i < N_TABLES; i++) {
    outfile.write((const char *) &save_tablesize, sizeof(save_tablesize));

    outfile.write((const char *) _counts[i],
		  sizeof(BoundedCounterType) * _tablebytes);
  }
  outfile.close();
}

void Hashbits::load(std::string infilename)
{
  if (_counts) {
    for (unsigned int i = 0; i < N_TABLES; i++) {
      delete _counts[i]; _counts[i] = NULL;
    }
  }
  
  unsigned int save_ksize = 0;
  unsigned long long save_tablesize = 0;

  ifstream infile(infilename.c_str(), ios::binary);
  infile.read((char *) &save_ksize, sizeof(save_ksize));
  _ksize = (WordLength) save_ksize;

  for (unsigned int i = 0; i < N_TABLES; i++) {
    infile.read((char *) &save_tablesize, sizeof(save_tablesize));

    _tablesize = (HashIntoType) save_tablesize;
    _tablebytes = _tablesize / 8 + 1;
    _counts[i] = new BoundedCounterType[_tablebytes];

    unsigned long long loaded = 0;
    while (loaded != _tablebytes) {
      infile.read((char *) _counts[i], _tablebytes - loaded);
      loaded += infile.gcount();	// do I need to do this loop?
    }
  }
  infile.close();
}

//////////////////////////////////////////////////////////////////////
// graph stuff

ReadMaskTable * Hashbits::filter_file_connected(const std::string &est,
                                                 const std::string &readsfile,
                                                 unsigned int total_reads)
{
   unsigned int read_num = 0;
   unsigned int n_kept = 0;
   unsigned long long int cluster_size;
   ReadMaskTable * readmask = new ReadMaskTable(total_reads);
   IParser* parser = IParser::get_parser(readsfile.c_str());


   std::string first_kmer = est.substr(0, _ksize);
   SeenSet keeper;
   calc_connected_graph_size(first_kmer.c_str(),
                             cluster_size,
                             keeper);

   while(!parser->is_complete())
   {
      std::string seq = parser->get_next_read().seq;

      if (readmask->get(read_num))
      {
         bool keep = false;

         HashIntoType h = 0, r = 0, kmer;
         kmer = _hash(seq.substr(0, _ksize).c_str(), _ksize, h, r);
         kmer = uniqify_rc(h, r);

         SeenSet::iterator i = keeper.find(kmer);
         if (i != keeper.end()) {
            keep = true;
         }

         if (!keep) {
            readmask->set(read_num, false);
         } else {
            n_kept++;
         }
      }

      read_num++;
   }

   return readmask;
}

void Hashbits::calc_connected_graph_size(const HashIntoType kmer_f,
					  const HashIntoType kmer_r,
					  unsigned long long& count,
					  SeenSet& keeper,
					  const unsigned long long threshold)
const
{
  HashIntoType kmer = uniqify_rc(kmer_f, kmer_r);
  const BoundedCounterType val = get_count(kmer);

  if (val == 0) {
    return;
  }

  // have we already seen me? don't count; exit.
  SeenSet::iterator i = keeper.find(kmer);
  if (i != keeper.end()) {
    return;
  }

  // keep track of both seen kmers, and counts.
  keeper.insert(kmer);
  count += 1;

  // are we past the threshold? truncate search.
  if (threshold && count >= threshold) {
    return;
  }

  // otherwise, explore in all directions.

  // NEXT.

  HashIntoType f, r;
  const unsigned int rc_left_shift = _ksize*2 - 2;

  f = ((kmer_f << 2) & bitmask) | twobit_repr('A');
  r = kmer_r >> 2 | (twobit_comp('A') << rc_left_shift);
  calc_connected_graph_size(f, r, count, keeper, threshold);

  f = ((kmer_f << 2) & bitmask) | twobit_repr('C');
  r = kmer_r >> 2 | (twobit_comp('C') << rc_left_shift);
  calc_connected_graph_size(f, r, count, keeper, threshold);

  f = ((kmer_f << 2) & bitmask) | twobit_repr('G');
  r = kmer_r >> 2 | (twobit_comp('G') << rc_left_shift);
  calc_connected_graph_size(f, r, count, keeper, threshold);

  f = ((kmer_f << 2) & bitmask) | twobit_repr('T');
  r = kmer_r >> 2 | (twobit_comp('T') << rc_left_shift);
  calc_connected_graph_size(f, r, count, keeper, threshold);

  // PREVIOUS.

  r = ((kmer_r << 2) & bitmask) | twobit_comp('A');
  f = kmer_f >> 2 | (twobit_repr('A') << rc_left_shift);
  calc_connected_graph_size(f, r, count, keeper, threshold);

  r = ((kmer_r << 2) & bitmask) | twobit_comp('C');
  f = kmer_f >> 2 | (twobit_repr('C') << rc_left_shift);
  calc_connected_graph_size(f, r, count, keeper, threshold);

  r = ((kmer_r << 2) & bitmask) | twobit_comp('G');
  f = kmer_f >> 2 | (twobit_repr('G') << rc_left_shift);
  calc_connected_graph_size(f, r, count, keeper, threshold);

  r = ((kmer_r << 2) & bitmask) | twobit_comp('T');
  f = kmer_f >> 2 | (twobit_repr('T') << rc_left_shift);
  calc_connected_graph_size(f, r, count, keeper, threshold);
}

void Hashbits::trim_graphs(const std::string infilename,
			    const std::string outfilename,
			    unsigned int min_size,
			    CallbackFn callback,
			    void * callback_data)
{
  IParser* parser = IParser::get_parser(infilename.c_str());
  unsigned int total_reads = 0;
  unsigned int reads_kept = 0;
  Read read;
  string seq;

  string line;
  ofstream outfile(outfilename.c_str());

  //
  // iterate through the FASTA file & consume the reads.
  //

  while(!parser->is_complete())  {
    read = parser->get_next_read();
    seq = read.seq;

    bool is_valid = check_read(seq);

    if (is_valid) {
      std::string first_kmer = seq.substr(0, _ksize);
      unsigned long long clustersize = 0;
      SeenSet keeper;
      calc_connected_graph_size(first_kmer.c_str(), clustersize, keeper,
				min_size);

      if (clustersize >= min_size) {
	outfile << ">" << read.name << endl;
	outfile << seq << endl;
	reads_kept++;
      }
    }
	       
    total_reads++;

    // run callback, if specified
    if (total_reads % CALLBACK_PERIOD == 0 && callback) {
      try {
	callback("trim_graphs", callback_data, total_reads, reads_kept);
      } catch (...) {
	delete parser;
	throw;
      }
    }
  }

  delete parser;
}

HashIntoType * Hashbits::graphsize_distribution(const unsigned int &max_size)
{
  HashIntoType * p = new HashIntoType[max_size];
  const unsigned char seen = 1 << 7;
  unsigned long long size;

  for (unsigned int i = 0; i < max_size; i++) {
    p[i] = 0;
  }

  for (HashIntoType i = 0; i < _tablesize; i++) {
    BoundedCounterType count = get_count(i);
    if (count && !(count & seen)) {
      std::string kmer = _revhash(i, _ksize);
      size = 0;

      SeenSet keeper;
      calc_connected_graph_size(kmer.c_str(), size, keeper, max_size);
      if (size) {
	if (size < max_size) {
	  p[size] += 1;
	}
      }
    }
  }

  return p;
}

void Hashbits::save_tagset(std::string outfilename)
{
  ofstream outfile(outfilename.c_str(), ios::binary);
  const unsigned int tagset_size = all_tags.size();
  HashIntoType * buf = new HashIntoType[tagset_size];

  outfile.write((const char *) &tagset_size, sizeof(tagset_size));

  unsigned int i = 0;
  for (SeenSet::iterator pi = all_tags.begin(); pi != all_tags.end();
	 pi++, i++) {
    buf[i] = *pi;
  }

  outfile.write((const char *) buf, sizeof(HashIntoType) * tagset_size);
  outfile.close();

  delete buf;
}

void Hashbits::load_tagset(std::string infilename)
{
  ifstream infile(infilename.c_str(), ios::binary);
  all_tags.clear();

  unsigned int tagset_size = 0;
  infile.read((char *) &tagset_size, sizeof(tagset_size));
  HashIntoType * buf = new HashIntoType[tagset_size];

  infile.read((char *) buf, sizeof(HashIntoType) * tagset_size);

  for (unsigned int i = 0; i < tagset_size; i++) {
    all_tags.insert(buf[i]);
  }
  
  delete buf;
}


// do_truncated_partition: progressive partitioning.
//   1) load all sequences, tagging first kmer of each
//   2) do a truncated BFS search for all connected tagged kmers & assign
//      partition ID.
//
// CTB note: for unlimited PARTITION_ALL_TAG_DEPTH, yields perfect clustering.

void Hashbits::do_truncated_partition(const std::string infilename,
				       CallbackFn callback,
				       void * callback_data)
{
  unsigned int total_reads = 0;

  IParser* parser = IParser::get_parser(infilename);
  Read read;
  string seq;
  bool is_valid;

  std::string first_kmer;
  HashIntoType kmer_f, kmer_r;
  SeenSet tagged_kmers;
  bool surrender;

  if (!partition) {
    partition = new SubsetPartition(this);
  }

  while(!parser->is_complete()) {
    // increment read number
    read = parser->get_next_read();
    total_reads++;

    seq = read.seq;

    check_and_process_read(seq, is_valid);

    if (is_valid) {
      first_kmer = seq.substr(0, _ksize);
      _hash(first_kmer.c_str(), _ksize, kmer_f, kmer_r);

      // find all tagged kmers within range.
      tagged_kmers.clear();
      surrender = false;
      partition->find_all_tags(kmer_f, kmer_r, tagged_kmers, surrender, true);

      // assign the partition ID
      partition->assign_partition_id(kmer_f, tagged_kmers, surrender);
      all_tags.insert(kmer_f);

      // run callback, if specified
      if (total_reads % CALLBACK_PERIOD == 0 && callback) {
	try {
	  callback("do_truncated_partition", callback_data, total_reads,
		   partition->next_partition_id);
	} catch (...) {
	  delete parser;
	  throw;
	}
      }
    }
  }

  delete parser;
}

// do_threaded_partition:

void Hashbits::do_threaded_partition(const std::string infilename,
				      CallbackFn callback,
				      void * callback_data)
{
  unsigned int total_reads = 0;

  IParser* parser = IParser::get_parser(infilename);
  Read read;
  string seq;
  bool is_valid;

  HashIntoType kmer_f, kmer_r;

  if (!partition) {
    partition = new SubsetPartition(this);
  }

  while(!parser->is_complete()) {
    // increment read number
    read = parser->get_next_read();
    total_reads++;

    seq = read.seq;

    is_valid = check_read(seq);
    if (is_valid) {
      const char * kmer_s = seq.c_str();
      HashIntoType kmer;
      HashIntoType last_overlap;

      bool found = false;

      for (unsigned int i = 0; i < seq.length() - _ksize + 1; i++) {
	_hash(kmer_s + i, _ksize, kmer_f, kmer_r);
	kmer = uniqify_rc(kmer_f, kmer_r);

	if (get_count(kmer)) {
	  if (!found) {
	    if (all_tags.find(kmer) == all_tags.end()) { // tag first intersect
	      all_tags.insert(kmer);
	      last_overlap = kmer;
	    }
	    found = true;
	  } else {
	    last_overlap = kmer;
	  }
	} else {		// no overlap
	  count(kmer);
	}
      }

      if (found) {		// tag last intersect
	all_tags.insert(last_overlap);
      } else {			// no intersect? insert first kmer.
	_hash(kmer_s, _ksize, kmer_f, kmer_r);
	kmer = uniqify_rc(kmer_f, kmer_r);
	all_tags.insert(kmer);
      }

      // run callback, if specified
      if (total_reads % CALLBACK_PERIOD == 0 && callback) {
	try {
	  callback("do_threaded_partition", callback_data, total_reads,
		   all_tags.size());
	} catch (...) {
	  delete parser;
	  throw;
	}
      }
    }
  }

  delete parser;
}

void SubsetPartition::count_partitions(unsigned int& n_partitions,
				       unsigned int& n_unassigned,
				       unsigned int& n_surrendered)
{
  n_partitions = 0;
  n_unassigned = 0;
  n_surrendered = 0;

  PartitionSet partitions;

  //
  // go through all the tagged kmers and count partitions/surrendered/orphan.
  //

  for (PartitionMap::const_iterator pi = partition_map.begin();
       pi != partition_map.end(); pi++) {
    PartitionID * partition_p = pi->second;
    if (partition_p) {
      partitions.insert(*partition_p);

      if (*partition_p == SURRENDER_PARTITION) {
	n_surrendered++;
      }
    }
    else {
      n_unassigned++;
    }
  }
  n_partitions = partitions.size();
}


unsigned int SubsetPartition::output_partitioned_file(const std::string infilename,
						      const std::string outputfile,
						      bool output_unassigned,
						      CallbackFn callback,
						      void * callback_data)
{
  IParser* parser = IParser::get_parser(infilename);
  ofstream outfile(outputfile.c_str());

  unsigned int total_reads = 0;
  unsigned int reads_kept = 0;
  unsigned int n_singletons = 0;

  PartitionSet partitions;

  Read read;
  string seq;

  std::string first_kmer;
  HashIntoType kmer_f, kmer_r, kmer;

  const unsigned char ksize = _ht->ksize();

  //
  // go through all the reads, and take those with assigned partitions
  // and output them.
  //

  while(!parser->is_complete()) {
    read = parser->get_next_read();
    seq = read.seq;

    if (_ht->check_read(seq)) {
      const char * kmer_s = seq.c_str();
      
      for (unsigned int i = 0; i < seq.length() - ksize + 1; i++) {
	_hash(kmer_s + i, ksize, kmer_f, kmer_r);
	kmer = uniqify_rc(kmer_f, kmer_r);

	// some partitioning schemes tag the first kmer_f; others label
	// *some* kmer in the read.  Output properly for both.

	if (partition_map.find(kmer_f) != partition_map.end()) {
	  kmer = kmer_f;
	  break;
	}
	if (partition_map.find(kmer) != partition_map.end()) {
	  break;
	}
      }

      PartitionID * partition_p = partition_map[kmer];
      PartitionID partition_id;
      if (partition_p == NULL ){
	partition_id = 0;
	n_singletons++;
      } else {
	partition_id = *partition_p;
	partitions.insert(partition_id);
      }

      // Is this a partition that has not been entirely explored? If so,
      // mark it.
      char surrender_flag = ' ';
      if (partition_id == SURRENDER_PARTITION) {
	surrender_flag = '*';
      }

      if (partition_id > 0 || output_unassigned) {
	outfile << ">" << read.name << "\t" << partition_id
		<< surrender_flag << "\n" 
		<< seq << "\n";
      }
	       
      total_reads++;

      // run callback, if specified
      if (total_reads % CALLBACK_PERIOD == 0 && callback) {
	try {
	  callback("output_partitions", callback_data,
		   total_reads, reads_kept);
	} catch (...) {
	  delete parser; parser = NULL;
	  outfile.close();
	  throw;
	}
      }
    }
  }

  delete parser; parser = NULL;

  // cout << partitions.size() << " + " << n_singletons << "\n";

  return partitions.size() + n_singletons;
}

///

bool SubsetPartition::_do_continue(const HashIntoType kmer,
				   const SeenSet& keeper)
{
  // have we already seen me? don't count; exit.
  SeenSet::iterator i = keeper.find(kmer);

  return (i == keeper.end());
}

bool SubsetPartition::_is_tagged_kmer(const HashIntoType kmer_f,
				      const HashIntoType kmer_r,
				      HashIntoType& tagged_kmer)
{
  SeenSet * tags = &_ht->all_tags;
  SeenSet::const_iterator fi = tags->find(kmer_f);
  if (fi != tags->end()) {
    tagged_kmer = kmer_f;
    return true;
  }

  fi = tags->find(kmer_r);
  if (fi != tags->end()) {
    tagged_kmer = kmer_r;
    return true;
  }

  return false;
}
		     

// used by do_truncated_partition

void SubsetPartition::find_all_tags(HashIntoType kmer_f,
				    HashIntoType kmer_r,
				    SeenSet& tagged_kmers,
				    bool& surrender,
				    bool do_initial_check)
{
  const HashIntoType bitmask = _ht->bitmask;

  HashIntoType tagged_kmer;
  if (do_initial_check && _is_tagged_kmer(kmer_f, kmer_r, tagged_kmer)) {
    if (partition_map[kmer_f] != NULL) {
      tagged_kmers.insert(tagged_kmer); // this might connect kmer_r and kmer_f
      return;
    }
  }

  HashIntoType f, r;
  bool first = true;
  NodeQueue node_q;
  const unsigned int rc_left_shift = _ht->ksize()*2 - 2;
  unsigned int total = 0;

  SeenSet keeper;		// keep track of traversed kmers

  // start breadth-first search.

  node_q.push(kmer_f);
  node_q.push(kmer_r);

  while(!node_q.empty()) {
    if (total > PARTITION_ALL_TAG_DEPTH && \
	node_q.size() > PARTITION_ALL_TAG_DEPTH) {
      surrender = true;
      break;
    }

    kmer_f = node_q.front();
    node_q.pop();
    kmer_r = node_q.front();
    node_q.pop();

    HashIntoType kmer = uniqify_rc(kmer_f, kmer_r);
    if (!_do_continue(kmer, keeper)) {
      continue;
    }

    // keep track of seen kmers
    keeper.insert(kmer);
    //    cout << "INSERT: " << _revhash(kmer, _ht->ksize()) << "=" << (int) (_ht->get_count(kmer)) << " xx " << kmer % _ht->n_entries() << " =\n";
    total++;

    // Is this a kmer-to-tag, and have we put this tag in a partition already?
    // Search no further in this direction.
    if (!first && _is_tagged_kmer(kmer_f, kmer_r, tagged_kmer)) {
      tagged_kmers.insert(tagged_kmer);
      first = false;
      continue;
    }

    //
    // Enqueue next set of nodes.
    //

    // NEXT.
    f = ((kmer_f << 2) & bitmask) | twobit_repr('A');
    r = kmer_r >> 2 | (twobit_comp('A') << rc_left_shift);
    if (_ht->get_count(uniqify_rc(f,r))) {
      node_q.push(f); node_q.push(r);
    }

    f = ((kmer_f << 2) & bitmask) | twobit_repr('C');
    r = kmer_r >> 2 | (twobit_comp('C') << rc_left_shift);
    if (_ht->get_count(uniqify_rc(f,r))) {
      node_q.push(f); node_q.push(r);
    }

    f = ((kmer_f << 2) & bitmask) | twobit_repr('G');
    r = kmer_r >> 2 | (twobit_comp('G') << rc_left_shift);
    if (_ht->get_count(uniqify_rc(f,r))) {
      node_q.push(f); node_q.push(r);
    }

    f = ((kmer_f << 2) & bitmask) | twobit_repr('T');
    r = kmer_r >> 2 | (twobit_comp('T') << rc_left_shift);
    if (_ht->get_count(uniqify_rc(f,r))) {
      node_q.push(f); node_q.push(r);
    }

    // PREVIOUS.
    r = ((kmer_r << 2) & bitmask) | twobit_comp('A');
    f = kmer_f >> 2 | (twobit_repr('A') << rc_left_shift);
    if (_ht->get_count(uniqify_rc(f,r))) {
      node_q.push(f); node_q.push(r);
    }

    r = ((kmer_r << 2) & bitmask) | twobit_comp('C');
    f = kmer_f >> 2 | (twobit_repr('C') << rc_left_shift);
    if (_ht->get_count(uniqify_rc(f,r))) {
      node_q.push(f); node_q.push(r);
    }
    
    r = ((kmer_r << 2) & bitmask) | twobit_comp('G');
    f = kmer_f >> 2 | (twobit_repr('G') << rc_left_shift);
    if (_ht->get_count(uniqify_rc(f,r))) {
      node_q.push(f); node_q.push(r);
    }

    r = ((kmer_r << 2) & bitmask) | twobit_comp('T');
    f = kmer_f >> 2 | (twobit_repr('T') << rc_left_shift);
    if (_ht->get_count(uniqify_rc(f,r))) {
      node_q.push(f); node_q.push(r);
    }

    first = false;
  }
}

///////////////////////////////////////////////////////////////////////

void SubsetPartition::do_partition(HashIntoType first_kmer,
				   HashIntoType last_kmer,
				   CallbackFn callback,
				   void * callback_data)
{
  unsigned int total_reads = 0;

  std::string kmer_s;
  HashIntoType kmer_f, kmer_r;
  SeenSet tagged_kmers;
  bool surrender;
  const unsigned char ksize = _ht->ksize();

  SeenSet::const_iterator si, end;

  if (first_kmer) {
    si = _ht->all_tags.find(first_kmer);
  } else {
    si = _ht->all_tags.begin();
  }
  if (last_kmer) {
    end = _ht->all_tags.find(last_kmer);
  } else {
    end = _ht->all_tags.end();
  }

  for (; si != end; si++) {
    total_reads++;

    kmer_s = _revhash(*si, ksize);
    _hash(kmer_s.c_str(), ksize, kmer_f, kmer_r);

    // find all tagged kmers within range.
    tagged_kmers.clear();
    surrender = false;
    find_all_tags(kmer_f, kmer_r, tagged_kmers, surrender, false);

    // assign the partition ID
    assign_partition_id(kmer_f, tagged_kmers, surrender);

    // run callback, if specified
    if (total_reads % CALLBACK_PERIOD == 0 && callback) {
      cout << "...subset-part " << first_kmer << "-" << last_kmer << ": " << total_reads << " <- " << next_partition_id << "\n";
#if 0 // @CTB
	try {
	  callback("do_subset_partition/read", callback_data, total_reads,
		   next_partition_id);
	} catch (...) {
	  delete parser;
	  throw;
	}
#endif // 0
      }
  }
}

//

void SubsetPartition::set_partition_id(std::string kmer_s, PartitionID p)
{
  HashIntoType kmer_f, kmer_r;
  assert(kmer_s.length() >= _ht->ksize());
  _hash(kmer_s.c_str(), _ht->ksize(), kmer_f, kmer_r);

  set_partition_id(kmer_f, p);
}

void SubsetPartition::set_partition_id(HashIntoType kmer_f, PartitionID p)
{
  PartitionPtrSet * s = reverse_pmap[p];
  PartitionID * pp = NULL;
  if (s == NULL) {
    s = new PartitionPtrSet();
    pp = new unsigned int(p);
    s->insert(pp);
    reverse_pmap[p] = s;
  } else {
    pp = *(s->begin());
  }
  partition_map[kmer_f] = pp;

  if (next_partition_id <= p) {
    next_partition_id = p + 1;
  }
}

PartitionID SubsetPartition::assign_partition_id(HashIntoType kmer_f,
						 SeenSet& tagged_kmers,
						 bool surrender)

{
  PartitionID return_val = 0; 
  PartitionID * pp = NULL;

  for (SeenSet::iterator si = tagged_kmers.begin(); si != tagged_kmers.end();
       si++) {
    PartitionID * pp = partition_map[*si];
    if (pp && *pp == SURRENDER_PARTITION) {
      surrender = true;
    }
  }

  // did we find a tagged kmer?
  if (surrender) {
    PartitionPtrSet * s = reverse_pmap[SURRENDER_PARTITION];
    PartitionPtrSet::iterator si = s->begin();

    PartitionID * surrender_pp = *si;
    

    SeenSet::iterator ii = tagged_kmers.begin();
    for (; ii != tagged_kmers.end(); ii++) {
      PartitionID * pp = partition_map[*ii];
      if (pp) {
	if (*pp != SURRENDER_PARTITION) {
	  _add_partition_ptr(surrender_pp, pp);
	}
      } else {
	partition_map[*ii] = surrender_pp;
      }
    }
    partition_map[kmer_f] = surrender_pp;
  }
  else if (tagged_kmers.size() >= 1) {
    pp = _reassign_partition_ids(tagged_kmers, kmer_f);
    return_val = *pp;
  } else {
    partition_map[kmer_f] = NULL;
    return_val = 0;
  }

  return return_val;
}

PartitionID * SubsetPartition::_reassign_partition_ids(SeenSet& tagged_kmers,
						     const HashIntoType kmer_f)
{
  SeenSet::iterator it = tagged_kmers.begin();
  unsigned int * this_partition_p = NULL;

  // find first assigned partition ID in tagged set
  while (it != tagged_kmers.end()) {
    this_partition_p = partition_map[*it];
    if (this_partition_p != NULL) {
      break;
    }
    it++;
  }

  // none? allocate!
  if (this_partition_p == NULL) {
    this_partition_p = new PartitionID(next_partition_id);
    next_partition_id++;

    PartitionPtrSet * s = new PartitionPtrSet();
    s->insert(this_partition_p);
    reverse_pmap[*this_partition_p] = s;
  }
  
  it = tagged_kmers.begin();
  for (; it != tagged_kmers.end(); ++it) {
    PartitionID * pp_id = partition_map[*it];
    if (pp_id == NULL) {
      partition_map[*it] = this_partition_p;
    } else {
      if (*pp_id != *this_partition_p) { // join partitions
	_add_partition_ptr(this_partition_p, pp_id);
      }
    }
  }

  assert(this_partition_p != NULL);
  partition_map[kmer_f] = this_partition_p;

  return this_partition_p;
}

///

void SubsetPartition::_add_partition_ptr(PartitionID *orig_pp, PartitionID *new_pp)
{
  PartitionPtrSet * s = reverse_pmap[*orig_pp];
  PartitionPtrSet * t = reverse_pmap[*new_pp];
  reverse_pmap.erase(*new_pp);
    
  for (PartitionPtrSet::iterator pi = t->begin(); pi != t->end(); pi++) {
    PartitionID * iter_pp;
    iter_pp = *pi;

    *iter_pp = *orig_pp;
    s->insert(iter_pp);
  }
  delete t;
}

PartitionID SubsetPartition::join_partitions(PartitionID orig, PartitionID join)
{
  if (orig == join) { return orig; }
  if (orig == 0 || join == 0) { return 0; }

  if (join == SURRENDER_PARTITION) {
    PartitionID tmp = join;
    join = orig;
    orig = tmp;
  }
    
  PartitionID * orig_pp = *(reverse_pmap[orig]->begin());
  PartitionID * join_pp = *(reverse_pmap[join]->begin());

  _add_partition_ptr(orig_pp, join_pp);

  return orig;
}

PartitionID SubsetPartition::get_partition_id(std::string kmer_s)
{
  HashIntoType kmer_f, kmer_r;
  assert(kmer_s.length() >= _ht->ksize());
  _hash(kmer_s.c_str(), _ht->ksize(), kmer_f, kmer_r);

  return get_partition_id(kmer_f);
}

PartitionID SubsetPartition::get_partition_id(HashIntoType kmer_f)
{
  if (partition_map.find(kmer_f) != partition_map.end()) {
    PartitionID * pp = partition_map[kmer_f];
    if (pp == NULL) {
      return 0;
    }
    return *pp;
  }
  return 0;
}


static void make_partitions_to_tags(PartitionMap& pmap,
				    PartitionsToTagsMap& pttmap)
{
  SeenSet * sp;
  HashIntoType tag;
  PartitionID p;

  for (PartitionMap::const_iterator pi = pmap.begin(); pi != pmap.end();
       pi++) {
    if (pi->second) {
      tag = pi->first;
      p = *(pi->second);

      if (p != SURRENDER_PARTITION) {
	sp = pttmap[p];
	if (sp == NULL) {
	  sp = new SeenSet();
	  pttmap[p] = sp;
	}
	sp->insert(tag);
      }
    }
  }
}

static void del_partitions_to_tags(PartitionsToTagsMap& pttmap)
{
  for (PartitionsToTagsMap::iterator pt = pttmap.begin();
       pt != pttmap.end(); pt++) {
    SeenSet * sp = pt->second;
    if (sp != NULL) {
      delete sp;
      pt->second = NULL;
    }
  }
}

#if 0

static void print_partition_set(PartitionSet& p)
{
  cout << "\tpartition set: ";
  for (PartitionSet::iterator pi = p.begin(); pi != p.end(); pi++) {
    cout << *pi << ", ";
  }
  cout << "\n";
}

static void print_tag_set(SeenSet& p)
{
  cout << "\ttag set: ";
  for (SeenSet::iterator si = p.begin(); si != p.end(); si++) {
    cout << *si << ", ";
  }
  cout << "\n";
}

#endif //0

static void get_new_tags_from_partitions(SeenSet& old_tags,
					 SeenSet& new_tags,
					 PartitionSet& new_partitions,
					 PartitionsToTagsMap& pttm)
{
  for (PartitionSet::const_iterator psi = new_partitions.begin();
       psi != new_partitions.end(); psi++) {
    PartitionID p = *psi;
    SeenSet * s = pttm[p];

    // should only happen for the incompletely traversed sets
    if (s == NULL) {
      assert(p == SURRENDER_PARTITION);
      continue;
    }

    assert(p != SURRENDER_PARTITION);
    
    for (SeenSet::const_iterator si = s->begin(); si != s->end(); si++) {
      SeenSet::const_iterator test = old_tags.find(*si);
      if (test == old_tags.end()) {
	new_tags.insert(*si);
      }
    }
  }
}

static void get_new_partitions_from_tags(PartitionSet& old_parts,
					 PartitionSet& new_parts,
					 SeenSet& new_tags,
					 PartitionMap& pmap)
{
  for (SeenSet::const_iterator si = new_tags.begin(); si != new_tags.end();
       si++) {
    PartitionID * pp = pmap[*si];
    if (pp != NULL) {
      PartitionID p = *pp;

      PartitionSet::const_iterator test = old_parts.find(p);
      if (test == old_parts.end()) {
	new_parts.insert(p);
      }
    }
  }
}

static void transfer_tags(SeenSet& from, SeenSet& to)
{
  for (SeenSet::const_iterator si = from.begin(); si != from.end(); si++) {
    to.insert(*si);
  }
  from.clear();
}

static void transfer_partitions(PartitionSet& from, PartitionSet& to)
{
  for (PartitionSet::const_iterator pi = from.begin(); pi != from.end();
       pi++) {
    to.insert(*pi);
  }
  from.clear();
}

void SubsetPartition::merge(SubsetPartition * other)
{
  assert (this != other);

  PartitionsToTagsMap subset_pttm, master_pttm;
  PartitionID * pp;
  PartitionID p;

  //
  // Convert the partition maps into something where we can do reverse
  // lookups of what tags belong to a given partition.
  //

  make_partitions_to_tags(other->partition_map, subset_pttm);
  make_partitions_to_tags(this->partition_map, master_pttm);

  // Run through all of the unique partitions in 'other' and find the
  // transitive overlaps with sets in 'this'.

  for (PartitionsToTagsMap::iterator ptti = subset_pttm.begin();
       ptti != subset_pttm.end(); ptti++) {
    p = ptti->first;
    SeenSet * these_tags = ptti->second;

    if (these_tags == NULL) {	// We NULL the values as we deal with parts.
      continue;
    }

    // OK, this partition has not yet been transferred.  DEAL.

    //
    // Here, we want to get all of the partitions connected to this
    // one in the master map and the subset map -- and do
    // transitively.  Loop until no more additional tags/partitions are found.
    //

    SeenSet old_tags;
    SeenSet new_tags;

    PartitionSet old_subset_partitions, new_subset_partitions;
    PartitionSet old_master_partitions, new_master_partitions;

    old_subset_partitions.insert(p);
    transfer_tags(*these_tags, new_tags);

    while(new_tags.size()) {
      // first, get partitions (and then tags) for partitions in the *master*
      // that overlap with any of the tags in this subset partition.
      get_new_partitions_from_tags(old_master_partitions,
				   new_master_partitions,
				   new_tags,
				   this->partition_map);
      transfer_tags(new_tags, old_tags);
      get_new_tags_from_partitions(old_tags, new_tags,
				   new_master_partitions, master_pttm);
      transfer_partitions(new_master_partitions, old_master_partitions);

      // ok, now get partitions (and then tags) for partitions in *subset*
      // that overlap with any of the tags from the master.

      get_new_partitions_from_tags(old_subset_partitions,
				   new_subset_partitions,
				   new_tags,
				   other->partition_map);
      transfer_tags(new_tags, old_tags);
      get_new_tags_from_partitions(old_tags, new_tags,
				   new_subset_partitions, subset_pttm);
      transfer_partitions(new_subset_partitions, old_subset_partitions);

      // aaaaaand.... iterate until no more tags show up!
    }

    // Deal with merging incompletely traversed sets...

    bool surrender = false;
    if (old_subset_partitions.find(SURRENDER_PARTITION) != 
	old_subset_partitions.end() ||
	old_master_partitions.find(SURRENDER_PARTITION) != 
	old_master_partitions.end()) {
      surrender = true;
    }

    // All right!  We've now got all the tags that we want to be part of
    // the master map partition; create or merge.

    PartitionPtrSet * pp_set = NULL;

    // No overlapping master partitions?  Create new.
    if (old_master_partitions.size() == 0 && !surrender) {
      pp = this->get_new_partition();
	
      pp_set = new PartitionPtrSet();
      pp_set->insert(pp);
      this->reverse_pmap[*pp] = pp_set;
    } else {
      // Overlapping?  Great!  Get the first master partition.
      if (surrender) {
	pp_set = this->reverse_pmap[SURRENDER_PARTITION];
      } else {
	PartitionSet::iterator psi = old_master_partitions.begin();
	pp_set = this->reverse_pmap[*psi];
      }
      assert(pp_set != NULL);
      pp = *(pp_set->begin());
    }

    if (surrender) {
      assert(*pp == SURRENDER_PARTITION);
    } else {
      assert(*pp != SURRENDER_PARTITION);
    }

    // Remove all of the SeenSets in the subset_pttm map for these
    // now-connected partitions.  (Also see NULL check at beginning of loop.)
    for (PartitionSet::iterator psi2 = old_subset_partitions.begin();
	 psi2 != old_subset_partitions.end(); psi2++) {
      SeenSet * sp = subset_pttm[*psi2];
      if (sp != NULL) {
	subset_pttm[*psi2] = NULL;
	delete sp;
      }
    }

    // Remap all of the tags in the master map. This has the side
    // benefit of condensing pretty much everything into a single
    // pointer...

    PartitionPtrSet remove_me;
    for (SeenSet::iterator si = old_tags.begin(); si != old_tags.end(); si++) {
      PartitionID * old_pp = this->partition_map[*si];

      if (old_pp && *old_pp == SURRENDER_PARTITION) {
	assert(surrender);
	continue;
      }

      if (old_pp == NULL) {	// no previously assigned partition? assign!
	this->partition_map[*si] = pp;
      } else if (old_pp != pp) { // Overwrite, but keep track of those to free.
	remove_me.insert(old_pp);
	this->partition_map[*si] = pp;

	// Note: even though there may be multiple PartitionID*s for
	// this partition that need reassigning, we don't need to
	// handle that in this loop; we're doing this systematically (and
	// the 'remove_me' set will clear out with duplicates).
      }
    }

    // Condense & reset the reverse_pmap for *this* entry, too.  These
    // PartitionID*s will have been added to 'remove_me' in the previous loop
    // already.
    if (this->reverse_pmap[*pp]->size() > 1) {
      pp_set = new PartitionPtrSet();
      pp_set->insert(pp);
      this->reverse_pmap[*pp] = pp_set;
    }

    // Finally: remove contents of remove_me & associated (obsolete)
    // reverse_pmap entries.
    for (PartitionPtrSet::iterator si = remove_me.begin();
	 si != remove_me.end(); si++) {
      PartitionID p = *(*si);
      assert (p != SURRENDER_PARTITION);

      pp_set = this->reverse_pmap[p];
      if (pp_set) {
	delete pp_set;
	this->reverse_pmap.erase(p);
      }
      delete *si;
    }
  }

  // OK, clean up the reverse maps.

  del_partitions_to_tags(subset_pttm);
  del_partitions_to_tags(master_pttm);

  //
  // Deal with the surrender partition!
  //

  // At this point, any partition in subset that overlapped with a surrendered
  // tag in master has already been surrendered.  Moreover, this has been
  // done transitively, so that if subset partition A connected master
  // partition B to another surrendered partition in the master, both A and
  // B were surrendered.
  //
  // What's left is to mark those tags present only in subset as
  // 'surrendered' -- they weren't in the subset_pttm to be transferred
  // as normal.  We also want to make sure that tags that are marked as
  // 'surrendered' in the subset partitionmap transfer that status to
  // the master partitionmap.
  // 

  PartitionID * surr_pp = *(this->reverse_pmap[SURRENDER_PARTITION]->begin());

  for (PartitionMap::iterator pi = other->partition_map.begin();
       pi != other->partition_map.end(); pi++) {
    if (pi->second && *(pi->second) == SURRENDER_PARTITION) {
      PartitionID * this_pp = this->partition_map[pi->first];
      if (!this_pp) {		// not present in master; copy
	this->partition_map[pi->first] = surr_pp;
      } else if (this_pp) {	// present in master; reassign.
	if (*this_pp != SURRENDER_PARTITION) {
	  this->_add_partition_ptr(surr_pp, this_pp);
	}
      }
    }
  }
}

//
// consume_fasta: consume a FASTA file of reads
//

void Hashbits::consume_fasta_and_tag(const std::string &filename,
				      unsigned int &total_reads,
				      unsigned long long &n_consumed,
				      CallbackFn callback,
				      void * callback_data)
{
  total_reads = 0;
  n_consumed = 0;

  IParser* parser = IParser::get_parser(filename.c_str());
  Read read;

  string seq = "";

  //
  // iterate through the FASTA file & consume the reads.
  //

  while(!parser->is_complete())  {
    read = parser->get_next_read();
    seq = read.seq;

    // yep! process.
    unsigned int this_n_consumed = 0;
    bool is_valid;

    this_n_consumed = check_and_process_read(seq, is_valid);
    n_consumed += this_n_consumed;
    if (is_valid) {
      string first_kmer = seq.substr(0, _ksize);
      HashIntoType kmer_f, kmer_r;
      _hash(first_kmer.c_str(), _ksize, kmer_f, kmer_r);

      all_tags.insert(kmer_f);
    }
	       
    // reset the sequence info, increment read number
    total_reads++;

    // run callback, if specified
    if (total_reads % CALLBACK_PERIOD == 0 && callback) {
      try {
        callback("consume_fasta_and_tag", callback_data, total_reads,
		 n_consumed);
      } catch (...) {
	delete parser;
        throw;
      }
    }
  }
  delete parser;
}

// load partition maps from & save to disk 

void SubsetPartition::save_partitionmap(string pmap_filename)
{
  ofstream outfile(pmap_filename.c_str(), ios::binary);
  char * buf = NULL;
  buf = new char[IO_BUF_SIZE];
  unsigned int n_bytes = 0;

  HashIntoType * kmer_p = NULL;
  PartitionID * pp;

  PartitionMap::const_iterator pi = partition_map.begin();
  for (; pi != partition_map.end(); pi++) {
    PartitionID p_id;

    HashIntoType kmer = pi->first;
    if (pi->second != NULL) {
      p_id = *(pi->second);

      kmer_p = (HashIntoType *) (buf + n_bytes);
      *kmer_p = kmer;
      n_bytes += sizeof(HashIntoType);
      pp = (PartitionID *) (buf + n_bytes);
      *pp = p_id;
      n_bytes += sizeof(PartitionID);

      if (n_bytes >= IO_BUF_SIZE - sizeof(HashIntoType) - sizeof(PartitionID)) {
	outfile.write(buf, n_bytes);
	n_bytes = 0;
      }
    }
  }
  if (n_bytes) {
    outfile.write(buf, n_bytes);
  }
  outfile.close();

  delete buf;
}
					 
void SubsetPartition::load_partitionmap(string infilename)
{
  ifstream infile(infilename.c_str(), ios::binary);
  char * buf = NULL;
  buf = new char[IO_BUF_SIZE];

  unsigned int n_bytes = 0;
  unsigned int loaded = 0;
  unsigned int remainder;

  assert(infile.is_open());

  PartitionSet partitions;

  HashIntoType * kmer_p = NULL;
  PartitionID * pp = NULL;

  remainder = 0;
  while (!infile.eof()) {
    unsigned int i;

    infile.read(buf + remainder, IO_BUF_SIZE - remainder);
    n_bytes = infile.gcount() + remainder;
    remainder = n_bytes % (sizeof(PartitionID) + sizeof(HashIntoType));
    n_bytes -= remainder;

    for (i = 0; i < n_bytes;) {
      // ignore kmer for this loop.
      i += sizeof(HashIntoType);
      pp = (PartitionID *) (buf + i);
      i += sizeof(PartitionID);

      partitions.insert(*pp);

      loaded++;
    }
    assert(i == n_bytes);
    memcpy(buf, buf + n_bytes, remainder);
  }

  PartitionID max_p_id = 1;
  PartitionPtrMap ppmap;
  for (PartitionSet::const_iterator si = partitions.begin();
      si != partitions.end(); si++) {
    if (*si == 0) {
      continue;
    }

    PartitionID * p = new PartitionID;
    *p = *(si);
    ppmap[*p] = p;

    PartitionPtrSet * s = new PartitionPtrSet();
    s->insert(p);
    reverse_pmap[*p] = s;

    if (max_p_id < *p) {
      max_p_id = *p;
    }
  }
  next_partition_id = max_p_id + 1;

  infile.clear();
  infile.seekg(0, ios::beg);

  remainder = 0;
  while (!infile.eof()) {
    unsigned int i;

    infile.read(buf + remainder, IO_BUF_SIZE - remainder);
    n_bytes = infile.gcount() + remainder;
    remainder = n_bytes % (sizeof(PartitionID) + sizeof(HashIntoType));
    n_bytes -= remainder;

    for (i = 0; i < n_bytes;) {
      kmer_p = (HashIntoType *) (buf + i);
      i += sizeof(HashIntoType);
      pp = (PartitionID *) (buf + i);
      i += sizeof(PartitionID);

      if (*pp == 0) {
	partition_map[*kmer_p] = NULL;
      } else {
	partition_map[*kmer_p] = ppmap[*pp];
      } 
    }
    assert(i == n_bytes);
    memcpy(buf, buf + n_bytes, remainder);
  }

  infile.close();
  delete buf; buf = NULL;
}


void SubsetPartition::_validate_pmap()
{
  // cout << "validating partition_map\n";

  for (PartitionMap::const_iterator pi = partition_map.begin();
       pi != partition_map.end(); pi++) {
    //HashIntoType kmer = (*pi).first;
    PartitionID * pp_id = (*pi).second;

    if (pp_id != NULL) {
      assert(*pp_id >= 1);
      assert(*pp_id < next_partition_id);
    }
  }

  // cout << "validating reverse_pmap -- st 1\n";
  for (ReversePartitionMap::const_iterator ri = reverse_pmap.begin();
       ri != reverse_pmap.end(); ri++) {
    PartitionID p = (*ri).first;
    PartitionPtrSet *s = (*ri).second;

    assert(s != NULL);

    for (PartitionPtrSet::const_iterator si = s->begin(); si != s->end();
	 si++) {
      PartitionID * pp;
      pp = *si;

      assert (p == *pp);
    }
  }
}

void SubsetPartition::_clear_partitions()
{
  for (ReversePartitionMap::iterator ri = reverse_pmap.begin();
       ri != reverse_pmap.end(); ri++) {
    PartitionPtrSet * s = (*ri).second;

    for (PartitionPtrSet::iterator pi = s->begin(); pi != s->end(); pi++) {
      PartitionID * pp = (*pi);
      delete pp;
    }
    delete s;
  }
  partition_map.clear();
  next_partition_id = 1;
}

//
// divide_tags_into_subsets - take all of the tags in 'all_tags', and
//   divide them into subsets (based on starting tag) of <= given size.
//

void Hashbits::divide_tags_into_subsets(unsigned int subset_size,
					 SeenSet& divvy)
{
  unsigned int i = 0;

  for (SeenSet::const_iterator si = all_tags.begin(); si != all_tags.end();
       si++) {
    if (i % subset_size == 0) {
      divvy.insert(*si);
      i = 0;
    }
    i++;
  }
}

//
// tags_to_map - convert the 'all_tags' set into a TagCountMap, connecting
//    each tag to a number (defaulting to zero).
//

void Hashbits::tags_to_map(TagCountMap& tag_map)
{
  for (SeenSet::const_iterator si = all_tags.begin(); si != all_tags.end();
       si++) {
    tag_map[*si] = 0;
  }
  cout << "TM size: " << tag_map.size() << "\n";
}

//
// maxify_partition_size -- build a PartitionCountMap (tracking the number
//   of tags included in each partition, by tag) and then check to see if
//   this particular subset has a larger partition size for that tag then
//   indicated in the TagCountMap.  Used to find the maximum included
//   partition size for a tag across multiple subsets.
//

void SubsetPartition::maxify_partition_size(TagCountMap& tag_map)
{
  PartitionCountMap partition_count;

  for (PartitionMap::const_iterator pi = partition_map.begin();
       pi != partition_map.end(); pi++) {
    PartitionID * pp = pi->second;
    if (pp) {
      partition_count[*pp]++;
    }
  }

  for (PartitionMap::const_iterator pi = partition_map.begin();
       pi != partition_map.end(); pi++) {
    PartitionID * pp = pi->second;
    if (pp) {    
      if (tag_map[pi->first] < partition_count[*pp]) {
	tag_map[pi->first] = partition_count[*pp];
      }
    }
  }
}

//
// discard_tags - remove tags from a TagCountMap if they have fewer than
//   threshold count tags in their partition.  Used to eliminate tags belonging
//   to small partitions.
//

void Hashbits::discard_tags(TagCountMap& tag_map, unsigned int threshold)
{
  SeenSet delete_me;

  for (TagCountMap::const_iterator ti = tag_map.begin(); ti != tag_map.end();
       ti++) {
    if (ti->second < threshold) {
      delete_me.insert(ti->first);
    }
  }

  for (SeenSet::const_iterator si = delete_me.begin(); si != delete_me.end();
       si++) {
    tag_map.erase(*si);
  }
}

//
// filter_against_tags - throw out elements of the partition map not present
//   in the TagCountMap.  Used to throw out tags belonging to small partitions.
//

void SubsetPartition::filter_against_tags(TagCountMap& tag_map)
{
  PartitionMap new_pmap;

  for (TagCountMap::const_iterator ti = tag_map.begin(); ti != tag_map.end();
       ti++) {
    if (partition_map.find(ti->first) != partition_map.end()) {
      new_pmap[ti->first] = partition_map[ti->first];
    }
  }

  cout << "OLD partition map size: " << partition_map.size() << "\n";
  cout << "NEW partition map size: " << new_pmap.size() << "\n";

  partition_map.swap(new_pmap);
  new_pmap.clear();
}

//
// consume_partitioned_fasta: consume a FASTA file of reads
//

void Hashbits::consume_partitioned_fasta(const std::string &filename,
					  unsigned int &total_reads,
					  unsigned long long &n_consumed,
					  CallbackFn callback,
					  void * callback_data)
{
  total_reads = 0;
  n_consumed = 0;

  IParser* parser = IParser::get_parser(filename.c_str());
  Read read;

  string seq = "";

  // reset the master subset partition
  delete partition;
  partition = new SubsetPartition(this);

  //
  // iterate through the FASTA file & consume the reads.
  //

  while(!parser->is_complete())  {
    read = parser->get_next_read();
    seq = read.seq;

    // yep! process.
    unsigned int this_n_consumed = 0;
    bool is_valid;

    this_n_consumed = check_and_process_read(seq, is_valid);
    n_consumed += this_n_consumed;
    if (is_valid) {
      // First, compute the tag (first k-mer)
      string first_kmer = seq.substr(0, _ksize);
      HashIntoType kmer_f, kmer_r;
      _hash(first_kmer.c_str(), _ksize, kmer_f, kmer_r);

      all_tags.insert(kmer_f);

      // Next, figure out the partition is (if non-zero), and save that.
      const char * s = read.name.c_str() + read.name.length() - 1;
      assert(*(s + 1) == (unsigned int) NULL);

      while(*s != '\t' && s >= read.name.c_str()) {
	s--;
      }

      if (*s == '\t') {
	PartitionID p = (PartitionID) atoi(s + 1);
	if (p > 0) {
	  partition->set_partition_id(kmer_f, p);
	}
      }
    }
	       
    // reset the sequence info, increment read number
    total_reads++;

    // run callback, if specified
    if (total_reads % CALLBACK_PERIOD == 0 && callback) {
      try {
        callback("consume_partitioned_fasta", callback_data, total_reads,
		 n_consumed);
      } catch (...) {
	delete parser;
        throw;
      }
    }
  }

  delete parser;
}