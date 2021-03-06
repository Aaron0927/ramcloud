/* Copyright (c) 2010-2014 Stanford University
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

#ifndef RAMCLOUD_OBJECTFINDER_H
#define RAMCLOUD_OBJECTFINDER_H

#include <boost/function.hpp>
#include <map>

#include "Common.h"
#include "CoordinatorClient.h"
#include "Key.h"
#include "Transport.h"
#include "Tablet.h"
#include "Indexlet.h"

namespace RAMCloud {

/**
 * Structure to define the key search value for the ObjectFinder map.
 */
struct TabletKey {
    uint64_t tableId;       // tableId of the tablet
    KeyHash keyHash;        // start key hash value

    /**
     * The operator < is overridden to implement the
     * correct comparison for the tableMap.
     */
    bool operator<(const TabletKey& key) const {
        return tableId < key.tableId ||
            (tableId == key.tableId && keyHash < key.keyHash);
    }
};

/**
 * Structure to extract and store information from Tablet ProtoBuffer.
 */
struct TabletWithLocator {
    Tablet tablet;
    //TODO(ashgup): Embed serviceLocator in tablet.h
    string serviceLocator;

    TabletWithLocator(Tablet tablet, string serviceLocator)
        : tablet(tablet)
        , serviceLocator(serviceLocator)
    {}
};

/**
 * This class maps from an object identifier (table and key) to a session
 * that can be used to communicate with the master that stores the object.
 * It retrieves configuration information from the coordinator and caches it.
 */
class ObjectFinder {
  public:
    class Indexlet; // forward declaration, see full declaration below
    class TableConfigFetcher; // forward declaration, see full declaration below

    explicit ObjectFinder(Context* context);

    Transport::SessionRef lookup(uint64_t tableId, const void* key,
                                 uint16_t keyLength);
    Transport::SessionRef lookup(uint64_t tableId, KeyHash keyHash);
    Transport::SessionRef lookup(uint64_t tableId, uint8_t indexId,
                                 const void* key, uint16_t keyLength);

    const Indexlet* lookupIndexlet(uint64_t tableId, uint8_t indexId,
                                   const void* key, uint16_t keyLength);
    const TabletWithLocator* lookupTablet(uint64_t table, KeyHash keyHash);

    void flush(uint64_t tableId);
    void flushSession(uint64_t tableId, KeyHash keyHash);
    void flushSession(uint64_t tableId, uint8_t indexId,
                      const void* key, uint16_t keyLength);
    void waitForTabletDown(uint64_t tableId);
    void waitForAllTabletsNormal(uint64_t tableId, uint64_t timeoutNs = ~0lu);

    // TODO(ashgup): This is duplicated from Indexlet. Try to use that directly.
    static int keyCompare(const void* key1, uint16_t keyLength1,
                                        const void* key2, uint16_t keyLength2);

    /*
     * Used only for debug purposes. This function created a string
     * representation of the tablets stored in tableMap
     */
    string debugString() const;

  PRIVATE:
    /**
     * Shared RAMCloud information.
     */
    Context* context;

    /**
     * tableMap provides a fast lookup for the current tablets being used.
     * It stores the tablets, so they can be fast accessed by having the 
     * TabletKey value <tableId, start_key_hash>.
     */
    std::map<TabletKey, TabletWithLocator> tableMap;
    typedef std::map<TabletKey, TabletWithLocator>::iterator TabletIter;

    //TODO(ashgup): later explore tuple including index key

    /**
     * tableIndexMap provides a fast lookup for the current indexes being used.
     * It stores the indexlets, so they can be fast accessed by having the 
     * index id and table id.
     */
    //TODO(ashgup): replace std::pair with custom structure
    std::multimap< std::pair<uint64_t, uint8_t>, Indexlet> tableIndexMap;
    typedef std::multimap< std::pair<uint64_t, uint8_t>,
                                    Indexlet>::iterator IndexletIter;

    /**
     * Update the local tablet map cache. Usually, calling
     * tableConfigFetcher.getTableConfig() is the same as calling
     * coordinator.getTableConfig(tableConfig). During unit tests, however,
     * this is swapped out with a mock implementation.
     */
    std::unique_ptr<ObjectFinder::TableConfigFetcher> tableConfigFetcher;

    DISALLOW_COPY_AND_ASSIGN(ObjectFinder);
};

/**
 * The following class holds information about a single indexlet of a given index
 * on a table.
 */
class ObjectFinder::Indexlet : public RAMCloud::Indexlet {
    public:
    Indexlet(const void *firstKey, uint16_t firstKeyLength,
             const void *firstNotOwnedKey, uint16_t firstNotOwnedKeyLength,
             ServerId serverId, string serviceLocator)
        : RAMCloud::Indexlet(firstKey, firstKeyLength, firstNotOwnedKey,
                   firstNotOwnedKeyLength)
        , serverId(serverId)
        , serviceLocator(serviceLocator)
    {}

    Indexlet(const Indexlet& indexlet)
        : RAMCloud::Indexlet(indexlet)
        , serverId(indexlet.serverId)
        , serviceLocator(indexlet.serviceLocator)
    {}

    /// The server id of the master owning this indexlet.
    ServerId serverId;

    /// The service locator for this indexlet.
    string serviceLocator;
};

/**
 * The interface for ObjectFinder::tableConfigFetcher. This is usually set to
 * RealTableConfigFetcher, which is defined in ObjectFinder.cc.
 */
class ObjectFinder::TableConfigFetcher {
  public:
    virtual ~TableConfigFetcher() {}
    /// See CoordinatorClient::getTableConfig.
    virtual void getTableConfig(
               uint64_t tableId,
               std::map<TabletKey, TabletWithLocator>* tableMap,
               std::multimap< std::pair<uint64_t, uint8_t>,
                                Indexlet>* tableIndexMap) = 0;
};

} // end RAMCloud

#endif  // RAMCLOUD_OBJECTFINDER_H
