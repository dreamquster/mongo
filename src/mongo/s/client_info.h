// @file s/client_info.h

/*
 *    Copyright (C) 2010 10gen Inc.
 *
 *    This program is free software: you can redistribute it and/or  modify
 *    it under the terms of the GNU Affero General Public License, version 3,
 *    as published by the Free Software Foundation.
 *
 *    This program is distributed in the hope that it will be useful,
 *    but WITHOUT ANY WARRANTY; without even the implied warranty of
 *    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *    GNU Affero General Public License for more details.
 *
 *    You should have received a copy of the GNU Affero General Public License
 *    along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 *    As a special exception, the copyright holders give permission to link the
 *    code of portions of this program with the OpenSSL library under certain
 *    conditions as described in each individual source file and distribute
 *    linked combinations including the program with the OpenSSL library. You
 *    must comply with the GNU Affero General Public License in all respects
 *    for all of the code used other than as permitted herein. If you modify
 *    file(s) with this exception, you may extend this exception to your
 *    version of the file(s), but you are not obligated to do so. If you do not
 *    wish to do so, delete this exception statement from your version. If you
 *    delete this exception statement from all source files in the program,
 *    then also delete it in the license file.
 */
#pragma once

#include "mongo/pch.h"

#include "mongo/db/client_basic.h"
#include "mongo/s/chunk.h"
#include "mongo/s/writeback_listener.h"
#include "mongo/s/write_ops/batch_write_exec.h"
#include "mongo/util/net/hostandport.h"

namespace mongo {

    class AbstractMessagingPort;

    /**
     * holds information about a client connected to a mongos
     * 1 per client socket
     * currently implemented with a thread local
     */
    class ClientInfo : public ClientBasic {
    public:
        ClientInfo(AbstractMessagingPort* messagingPort);
        ~ClientInfo();

        /** new request on behalf of a client, adjusts internal state */
        void newPeerRequest( const HostAndPort& peer );

        /** new request not associated (yet or ever) with a client */
        void newRequest();


        /** client disconnected */
        void disconnect();

        bool hasRemote() const { return true; }

        /**
         * @return remote socket address of the client
         */
        HostAndPort getRemote() const { return _remote; }

        /**
         * notes that this client use this shard
         * keeps track of all shards accessed this request
         */
        void addShardHost( const string& shardHost );

        /**
         * Notes that this client wrote to these particular hosts with write commands.
         */
        void addHostOpTimes( const HostOpTimeMap& hostOpTimes );

        /**
         * gets shards used on the previous request
         */
        set<string>* getPrevShardHosts() const { return &_prev->shardHostsWritten; }

        /**
         * Gets the shards, hosts, and opTimes the client last wrote to with write commands.
         */
        const map<string, OpTime>& getPrevHostOpTimes() const {
            return _prev->hostOpTimes;
        }

        /**
         * gets all shards we've accessed since the last time we called clearSinceLastGetError
         */
        const set<string>& sinceLastGetError() const { return _sinceLastGetError; }

        /**
         * clears list of shards we've talked to
         */
        void clearSinceLastGetError() { _sinceLastGetError.clear(); }


        /**
         * resets the information stored for the current request
         */
        void clearRequestInfo(){ _cur->clear(); }

        void disableForCommand();

        /**
         * Uses GLE and the shard hosts and opTimes last written by write commands to enforce a
         * write concern.
         *
         * Returns true if write concern was enforced, false with errMsg if not.
         */
        bool enforceWriteConcern( const string& dbName, const BSONObj& options, string* errMsg );

        /**
         * calls getLastError
         * resets shards since get last error
         * @return if the command was ok or if there was an error
         */
        bool getLastError( const string& dbName,
                           const BSONObj& options ,
                           BSONObjBuilder& result ,
                           string& errmsg,
                           bool fromWriteBackListener = false );

        /** @return if its ok to auto split from this client */
        bool autoSplitOk() const { return _autoSplitOk && Chunk::ShouldAutoSplit; }

        void noAutoSplit() { _autoSplitOk = false; }

        // Returns whether or not a ClientInfo for this thread has already been created and stored
        // in _tlInfo.
        static bool exists();
        // Gets the ClientInfo object for this thread from _tlInfo. If no ClientInfo object exists
        // yet for this thread, it creates one.
        static ClientInfo * get(AbstractMessagingPort* messagingPort = NULL);
        // Creates a ClientInfo and stores it in _tlInfo
        static ClientInfo* create(AbstractMessagingPort* messagingPort);

    private:

        int _id; // unique client id
        HostAndPort _remote; // server:port of remote socket end

        struct RequestInfo {

            void clear() {
                shardHostsWritten.clear();
                hostOpTimes.clear();
            }

            set<string> shardHostsWritten;
            map<string, OpTime> hostOpTimes;
        };

        // we use _a and _b to store info from the current request and the previous request
        // we use 2 so we can flip for getLastError type operations

        RequestInfo _a; // actual set for _cur or _prev
        RequestInfo _b; //   "

        RequestInfo* _cur; // pointer to _a or _b depending on state
        RequestInfo* _prev; //  ""


        set<string> _sinceLastGetError; // all shards accessed since last getLastError

        int _lastAccess;
        bool _autoSplitOk;

        static boost::thread_specific_ptr<ClientInfo> _tlInfo;
    };


}
