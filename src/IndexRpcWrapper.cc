/* Copyright (c) 2014 Stanford University
 *
 * Permission to use, copy, modify, and distribute this software for any purpose
 * with or without fee is hereby granted, provided that the above copyright
 * notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR(S) DISCLAIM ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL AUTHORS BE LIABLE FOR ANY
 * SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER
 * RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF
 * CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN
 * CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include "IndexRpcWrapper.h"
#include "Logger.h"
#include "MasterService.h"
#include "ShortMacros.h"

namespace RAMCloud {

/**
 * Constructor for IndexRpcWrapper objects.
 * \param ramcloud
 *      The RAMCloud object that governs this RPC.
 * \param tableId
 *      The table containing the desired object.
 * \param indexId
 *      Id of the index for which keys have to be compared.
 * \param key
 *      The location of key determines which server this rpc will be sent to.
 *      It does not necessarily have to be null terminated.  The caller must
 *      ensure that the storage for this key is unchanged through the life of
 *      the RPC.
 * \param keyLength
 *      Length in bytes of the key.
 * \param responseHeaderLength
 *      The size of header expected in the response for this RPC;
 *      incoming responses will be checked here to ensure that they
 *      contain at least this much data, and a pointer to the header
 *      will be stored in the responseHeader for the use of wrapper
 *      subclasses.
 * 
 * \param responseBuffer
 *      Optional client-supplied buffer to use for the RPC's response;
 *      if NULL then we use a built-in buffer.
 */
IndexRpcWrapper::IndexRpcWrapper(
            RamCloud* ramcloud, uint64_t tableId, uint8_t indexId,
            const void* key, uint16_t keyLength,
            uint32_t responseHeaderLength, Buffer* responseBuffer)
    : RpcWrapper(responseHeaderLength, responseBuffer)
    , context(ramcloud->clientContext)
    , objectFinder(&ramcloud->objectFinder)
    , tableId(tableId)
    , indexId(indexId)
    , key(key)
    , keyLength(keyLength)
{
}

/**
 * Constructor for IndexRpcWrapper objects.
 * 
 * \param master
 *      The master that governs this RPC.
 * \param tableId
 *      The table containing the desired object.
 * \param indexId
 *      Id of the index for which keys have to be compared.
 * \param key
 *      The location of key determines which server this rpc will be sent to.
 *      It does not necessarily have to be null terminated.  The caller must
 *      ensure that the storage for this key is unchanged through the life of
 *      the RPC.
 * \param keyLength
 *      Length in bytes of the key.
 * \param responseHeaderLength
 *      The size of header expected in the response for this RPC;
 *      incoming responses will be checked here to ensure that they
 *      contain at least this much data, and a pointer to the header
 *      will be stored in the responseHeader for the use of wrapper
 *      subclasses.
 * 
 * \param responseBuffer
 *      Optional client-supplied buffer to use for the RPC's response;
 *      if NULL then we use a built-in buffer.
 */
IndexRpcWrapper::IndexRpcWrapper(
            MasterService* master, uint64_t tableId, uint8_t indexId,
            const void* key, uint16_t keyLength,
            uint32_t responseHeaderLength, Buffer* responseBuffer)
    : RpcWrapper(responseHeaderLength, responseBuffer)
    , context(master->context)
    , objectFinder(&master->objectFinder)
    , tableId(tableId)
    , indexId(indexId)
    , key(key)
    , keyLength(keyLength)
{
}

// See RpcWrapper for documentation.
bool
IndexRpcWrapper::checkStatus()
{
    if (responseHeader->status == STATUS_UNKNOWN_INDEXLET) {
        // The index entry isn't where we thought it should be. Refresh our
        // configuration cache and try again.
        LOG(NOTICE,
            "Server %s doesn't store given secondary key "
                "for table %lu, index id %u; refreshing object map",
            session->getServiceLocator().c_str(), tableId, indexId);
        objectFinder->flush(tableId);
        send();
        return false;
    }
    return true;
}

// See RpcWrapper for documentation.
bool
IndexRpcWrapper::handleTransportError()
{
    // There was a transport-level failure. Flush cached state related
    // to this session, and related to the object mapping for our object.
    // Then retry.
    objectFinder->flushSession(tableId, indexId, key, keyLength);
    session = NULL;
    objectFinder->flush(tableId);
    send();
    return false;
}

// See RpcWrapper for documentation.
void
IndexRpcWrapper::send()
{
    session = objectFinder->lookup(tableId, indexId, key, keyLength);
    if (session == Transport::SessionRef()) {
        // This index doesn't exist. No need to take any action.
        state = CANCELED;
        return;
    }
    state = IN_PROGRESS;
    session->sendRequest(&request, response, this);
}

/**
 * This method provides a simple implementation of \c wait that
 * doesn't do any processing of the result; it just waits for completion
 * and checks for errors.
 * 
 * \return
 *      Boolean value false if the rpc was canceled, true otherwise.
 */
bool
IndexRpcWrapper::waitForIndexRpc()
{
    try {
        RpcWrapper::simpleWait(context->dispatch);
    } catch (RAMCloud::RpcCanceledException& e) {
        // This index doesn't exist. No need to take any action.
        // Other exceptions can get propagated.
        return false;
    }
    return true;
}

} // namespace RAMCloud
