/* Copyright (c) 2010-2012 Stanford University
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

#include "Common.h"
#include "BitOps.h"
#include "Service.h"
#include "TransportManager.h"

#ifndef RAMCLOUD_BINDTRANSPORT_H
#define RAMCLOUD_BINDTRANSPORT_H

namespace RAMCloud {

/**
 * This class defines an implementation of Transport that allows unit
 * tests to run without a network or a remote counterpart (it injects RPCs
 * directly into a Service instance's #dispatch() method).
 */
struct BindTransport : public Transport {
    // The following utility class keeps track of a collection of
    // services all associated with the same service locator (e.g.
    // the services that would be contained in a single server).
    struct ServiceArray {
        Service* services[WireFormat::INVALID_SERVICE];
    };

    explicit BindTransport(Context* context, Service* service = NULL)
        : context(context), services(), abortCounter(0), errorMessage()
    {
        if (service)
            addService(*service, "mock:", WireFormat::MASTER_SERVICE);
    }

    string
    getServiceLocator() {
        return "mock:";
    }

    void
    addService(Service& service, const string locator,
            WireFormat::ServiceType type) {
        services[locator].services[type] = &service;
    }

    Transport::SessionRef
    getSession(const ServiceLocator& serviceLocator, uint32_t timeoutMs = 0) {
        const string& locator = serviceLocator.getOriginalString();
        ServiceMap::iterator it = services.find(locator);
        if (it == services.end()) {
            throw TransportException(HERE, format("Unknown mock host: %s",
                                                  locator.c_str()));
        }
        return new BindSession(*this, &it->second, locator);
    }

    Transport::SessionRef
    getSession() {
        return context->transportManager->getSession("mock:");
    }

    struct BindServerRpc : public ServerRpc {
        BindServerRpc() {}
        void sendReply() {}
        DISALLOW_COPY_AND_ASSIGN(BindServerRpc);
    };

    struct BindSession : public Session {
      public:
        explicit BindSession(BindTransport& transport, ServiceArray* services,
                             const string& locator)
            : transport(transport), services(services),
            lastRequest(NULL), lastResponse(NULL), lastNotifier(NULL),
            dontNotify(false)\
        {
            setServiceLocator(locator);
        }

        void abort() {}
        void cancelRequest(RpcNotifier* notifier) {}
        string getRpcInfo()
        {
            if (lastNotifier == NULL)
                return "no active RPCs via BindTransport";
            return format("%s via BindTransport",
                    WireFormat::opcodeSymbol(lastRequest));
        }
        void sendRequest(Buffer* request, Buffer* response,
                         RpcNotifier* notifier)
        {
            response->reset();
            lastRequest = request;
            lastResponse = response;
            lastNotifier = notifier;
            Service::Rpc rpc(NULL, request, response);
            if (transport.abortCounter > 0) {
                transport.abortCounter--;
                if (transport.abortCounter == 0) {
                    // Simulate a failure of the server to respond.
                    notifier->failed();
                    return;
                }
            }
            if (transport.errorMessage != "") {
                notifier->failed();
                transport.errorMessage = "";
                return;
            }
            const WireFormat::RequestCommon* header;
            header = request->getStart<WireFormat::RequestCommon>();
            if ((header == NULL) ||
                    (header->service >= WireFormat::INVALID_SERVICE)) {
                throw ServiceNotAvailableException(HERE);
            }
            Service* service = services->services[header->service];
            if (service == NULL) {
                throw ServiceNotAvailableException(HERE);
            }
            service->handleRpc(&rpc);
            if (!dontNotify) {
                notifier->completed();
                lastNotifier = NULL;
            }
        }
        BindTransport& transport;

        // Points to an array holding one of each of the available services.
        ServiceArray* services;

        // The request and response buffers from the last call to sendRequest
        // for this session.
        Buffer *lastRequest, *lastResponse;

        // Notifier from the last call to sendRequest, if that call hasn't
        // yet been responded to.
        RpcNotifier *lastNotifier;

        // If the following variable is set to true by testing code, then
        // sendRequest does not immediately signal completion of the RPC.
        // It does complete the RPC, but returns without calling the
        // notifier, leaving it to testing code to invoke the notifier
        // explicitly to complete the call (the testing code can also
        // modify the response).
        bool dontNotify;

        DISALLOW_COPY_AND_ASSIGN(BindSession);
    };

    // Shared RAMCloud information.
    Context* context;

    typedef std::map<const string, ServiceArray> ServiceMap;
    ServiceMap services;

    // The following value is used to simulate server timeouts.
    int abortCounter;

    /**
     * If this is set to a non-empty value then the next RPC will
     * fail immediately.
     */
    string errorMessage;

    DISALLOW_COPY_AND_ASSIGN(BindTransport);
};

}  // namespace RAMCloud

#endif
