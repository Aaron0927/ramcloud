/* Copyright (c) 2011 Stanford University
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR(S) DISCLAIM ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL AUTHORS BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#ifndef RAMCLOUD_LOGITERATOR_H
#define RAMCLOUD_LOGITERATOR_H

#include <stdint.h>
#include <vector>
#include "Log.h"
#include "SegmentIterator.h"

namespace RAMCloud {

/**
 * The LogIterator provides the necessary state and methods to step through an
 * entire log, entry by entry. In conjunction with the SegmentManager class, it
 * hides the complexities of dealing with a log that is in constant flux due to
 * new appends and log cleaning. This class is primarily used by the tablet
 * migration mechanism, though it could be used for many other purposes, such as
 * scrubbing memory for bit errors.
 *
 * It is important to understand that in order to deal with concurrent appends
 * and cleaning, the instantation of a LogIterator object essentially stops the
 * log from reclaiming memory due to cleaning. Furthermore, once the head log
 * segment is reached, further appends to it are paused until the iterator is
 * destroyed. It is important, therefore, that log iterators are not longer
 * lived than they need to be and especially that iteration of the head segment
 * completes quickly.
 */
class LogIterator {
  PUBLIC:
    explicit LogIterator(Log& log);
    ~LogIterator();

    bool isDone();
    void next();
    bool onHead();
    LogEntryType getType();
    uint32_t getLength();
    uint32_t appendToBuffer(Buffer& buffer);
    uint32_t setBufferTo(Buffer& buffer);

  PRIVATE:
    /**
     * Comparison functor used to sort Segments by ID in descending order.
     */
    struct SegmentIdLessThan {
      public:
        bool
        operator()(const LogSegment* a, const LogSegment* b)
        {
            return a->id > b->id;
        }
    };

    void populateSegmentList(uint64_t nextSegmentId);

    /// Reference to the Log we're iterating.
    Log& log;

    /// Current list of segments to iterate over. Once exhausted, the list may
    /// need to be updated (since the log could have advanced forward).
    LogSegmentVector segmentList;

    /// SegmentIterator for the Segment we're currently iterating over.
    Tub<SegmentIterator> currentIterator;

    /// Identifier of the Segment currently being iterated over.
    uint64_t currentSegmentId;

    /// Indication that the head is locked and must be unlocked on destruction.
    bool headLocked;

    DISALLOW_COPY_AND_ASSIGN(LogIterator);
};

} // namespace

#endif // !RAMCLOUD_LOGITERATOR_H
