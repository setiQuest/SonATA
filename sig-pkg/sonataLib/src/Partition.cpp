/*******************************************************************************

 File:    Partition.cpp
 Project: OpenSonATA
 Authors: The OpenSonATA code is the result of many programmers
          over many years

 Copyright 2011 The SETI Institute

 OpenSonATA is free software: you can redistribute it and/or modify
 it under the terms of the GNU General Public License as published by
 the Free Software Foundation, either version 3 of the License, or
 (at your option) any later version.
 
 OpenSonATA is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License for more details.
 
 You should have received a copy of the GNU General Public License
 along with OpenSonATA.  If not, see<http://www.gnu.org/licenses/>.
 
 Implementers of this code are requested to include the caption
 "Licensed through SETI" with a link to setiQuest.org.
 
 For alternate licensing arrangements, please contact
 The SETI Institute at www.seti.org or setiquest.org. 

*******************************************************************************/

//
// Fixed-size memory partition set
//
// Header: $
//
#include <iostream>
#include "Err.h"
#include "ErrMsg.h"
#include "Partition.h"

namespace sonata_lib {

size_t
MemBlk::getBlkSize()
{
	return (parent->getBlkSize());
}

void
MemBlk::free()
{
	parent->free(this);
}

Partition::Partition(size_t size_, uint32_t nBlks_): data(0), blocks(nBlks_),
		allocs(0), frees(0), size(size_), smallest(size_), largest(0),
		blks(0), llock("llock")
{
	// allocate the data space
	data = (uint8_t *) fftwf_malloc(size * blocks);
	Assert(data);
	memset(data, 0, size * blocks);

	// allocate the blocks
	blks = new MemBlk[blocks];
	Assert(blks);

	// clear the memory
	for (int32_t i = 0; i < blocks; ++i) {
		MemBlk *blk = &blks[i];
		blk->init(this, &data[i*size]);
		list.push_back(blk);
	}
}

Partition::~Partition()
{
	list.clear();
	delete [] blks;
	fftwf_free(data);
}

MemBlk *
Partition::alloc()
{
	MemBlk *blk = 0;

	lock();
	if (!list.empty()) {
		blk = list.front();
		list.pop_front();
		++allocs;
	}
	unlock();
	return (blk);
}

//
// free: add the block to the end of the free list
//
void
Partition::free(MemBlk *blk_)
{
	lock();
	list.push_back(blk_);
	frees++;
	unlock();
}

PartitionSet *PartitionSet::instance = 0;

PartitionSet *
PartitionSet::getInstance()
{
	static Lock l;
	l.lock();
	if (!instance)
		instance = new PartitionSet();
	l.unlock();
	return (instance);
}

PartitionSet::PartitionSet(): largest(0), plock("PLock")
{
	errList = ErrMsg::getInstance();
	Assert(errList);
}

PartitionSet::~PartitionSet()
{
}

//
// Add a partition to the set.
//
void
PartitionSet::addPartition(Partition *partition_)
{
	lock();
	list.insert(std::make_pair(partition_->getBlkSize(), partition_));
	unlock();
}

MemBlk *
PartitionSet::alloc(size_t size)
{
	MemBlk *blk = 0;

	lock();
	if (size > largest) {
		largest = size;
		std::cout << "request = " << size << " bytes" << std::endl;
	}
	// find the first partition with a block size which is
	// large enough to hold the request
	PartitionList::iterator p = list.lower_bound(size);
	// if a partition has a large enough block size, but
	// is empty, try a larger one
	while ((p != list.end()) && !(blk = p->second->alloc()))
		++p;
	unlock();
	if (!blk) {
#ifndef notdef
		ErrStr(size, "allocation size");
		string &errMsg = errList->getErrMsg(ERR_MAF);
		FatalStr(ERR_MAF, errMsg.c_str());
#else
		// force a core dump here if there's no partition available
		std::cout << "alloc size = " << size << std::endl;
		char *p = 0;
		*p = 0;
#endif
	}
	return (blk);
}

}
