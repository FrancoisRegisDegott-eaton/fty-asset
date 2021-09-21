/*  =========================================================================
    fty_common_asset - General asset representation

    Copyright (C) 2019 - 2020 Eaton

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License along
    with this program; if not, write to the Free Software Foundation, Inc.,
    51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
    =========================================================================
*/

#ifndef FTY_COMMON_ASSET_H_INCLUDED
#define FTY_COMMON_ASSET_H_INCLUDED

#include <cxxtools/serializationinfo.h>
#include <string>

// fwd declaration
struct fty_proto_t;

// clang-format off
using AttributesMap = std::map<std::string,std::string>;

namespace fty
{
    /*
     * \brief Class that provides C++ interface for assets
     * Serialization and deserialization from all objects should be handled elsewhere due to dependencies
     */
    class BasicAsset {
        public:
            /// List of valid asset statuses
            enum Status {
                Active, Nonactive
            };
        protected:
            /// internal identification string (iname)
            std::string id_;
            Status status_;
            std::pair<uint16_t, uint16_t> type_subtype_;
        private:
            /// asset types string reprezentation
            std::string typeToString (uint16_t type) const;
            /// asset types from string reprezentation
            uint16_t stringToType (const std::string & type) const;
            /// asset subtypes string reprezentation
            std::string subtypeToString (uint16_t subtype) const;
            /// asset subtypes from string reprezentation
            uint16_t stringToSubtype (const std::string & subtype) const;
            /// asset statuses string reprezentation
            std::string statusToString (Status status) const;
            /// asset statuses from string reprezentation
            Status stringToStatus (const std::string & status) const;
        public:
            // ctors, dtors, =
            explicit BasicAsset (const std::string & id, const std::string & status, const std::string & type, const std::string & subtype);
            explicit BasicAsset (fty_proto_t *msg);
            explicit BasicAsset (const cxxtools::SerializationInfo & si);
            //BasicAsset () = delete;
            BasicAsset ();
            BasicAsset (const BasicAsset & asset) = default;
            BasicAsset (BasicAsset && asset) = default;
            BasicAsset & operator= (const BasicAsset & asset) = default;
            BasicAsset & operator= (BasicAsset && asset) = default;
            virtual ~BasicAsset () = default;
            // handling
            /// full equality check
            bool operator== (const BasicAsset &asset) const;
            bool operator!= (const BasicAsset &asset) const;
            /// comparator for simplified equality check
            bool compare (const BasicAsset &asset) const;
            // getters/setters
            std::string getId () const;
            Status getStatus () const;
            std::string getStatusString () const;
            uint16_t getType () const;
            std::string getTypeString () const;
            uint16_t getSubtype () const;
            std::string getSubtypeString () const;
            void setStatus (const std::string & status);
            void setType (const std::string & type);
            void setSubtype (const std::string & subtype);

            void deserialize (const cxxtools::SerializationInfo & si);
            void serialize (cxxtools::SerializationInfo & si) const;
            std::string toJson() const;

            bool isPowerAsset () const;
    };

    /// extends basic asset with location and user identification and priority
    class ExtendedAsset : public BasicAsset {
        protected:
            /// external name (ename) provided by user
            std::string name_;
            /// direct parent iname
            std::string parent_id_;
            /// priority 1..5 (1 is most, 5 is least)
            uint8_t priority_ = 0;
        public:
            // ctors, dtors, =
            explicit ExtendedAsset (const std::string & id, const std::string & status, const std::string & type, const std::string & subtype,
                    const std::string & name, const std::string & parent_id, int priority);
            explicit ExtendedAsset (const std::string & id, const std::string & status, const std::string & type, const std::string & subtype,
                    const std::string & name, const std::string & parent_id, const std::string & priority);
            explicit ExtendedAsset (fty_proto_t *msg);
            explicit ExtendedAsset (const cxxtools::SerializationInfo & si);
            //ExtendedAsset () = delete;
            ExtendedAsset ();
            ExtendedAsset (const ExtendedAsset & asset) = default;
            ExtendedAsset (ExtendedAsset && asset) = default;
            ExtendedAsset & operator= (const ExtendedAsset & asset) = default;
            ExtendedAsset & operator= (ExtendedAsset && asset) = default;
            virtual ~ExtendedAsset () = default;
            // handling
            bool operator== (const ExtendedAsset &asset) const;
            bool operator!= (const ExtendedAsset &asset) const;
            // getters/setters
            std::string getName () const;
            std::string getParentId () const;
            std::string getPriorityString () const;
            int getPriority () const;
            void setName (const std::string & name);
            void setParentId (const std::string & parent_id);
            void setPriority (int priority);
            void setPriority (const std::string & priority);

            void deserialize (const cxxtools::SerializationInfo & si);
            void serialize (cxxtools::SerializationInfo & si) const;
            std::string toJson() const;
    };

    /// provide full details about the asset without specifying asset type
    class FullAsset : public ExtendedAsset {
        public:
            typedef std::map<std::string, std::string> HashMap;
        protected:
            /// aux map storage (parents, etc)
            HashMap aux_;
            /// ext map storage (asset-specific values)
            HashMap ext_;
        public:
            // ctors, dtors, =
            explicit FullAsset (const std::string & id, const std::string & status, const std::string & type, const std::string & subtype,
                    const std::string & name, const std::string & parent_id, int priority, HashMap aux, HashMap ext);
            explicit FullAsset (const std::string & id, const std::string & status, const std::string & type, const std::string & subtype,
                    const std::string & name, const std::string & parent_id, const std::string & priority, HashMap aux, HashMap ext);
            explicit FullAsset (fty_proto_t *msg);
            explicit FullAsset (const cxxtools::SerializationInfo & si);
            //FullAsset () = delete;
            FullAsset ();
            FullAsset (const FullAsset & asset) = default;
            FullAsset (FullAsset && asset) = default;
            FullAsset & operator= (const FullAsset & asset) = default;
            FullAsset & operator= (FullAsset && asset) = default;
            virtual ~FullAsset () = default;
            // handling
            bool operator== (const FullAsset &asset) const;
            bool operator!= (const FullAsset &asset) const;
            // getters/setters
            HashMap getAux () const;
            HashMap getExt () const;
            void setAux (HashMap aux);
            void setExt (HashMap ext);
            std::string getAuxItem (const std::string &key) const;
            std::string getExtItem (const std::string &key) const;
            /// get item from either aux or ext
            std::string getItem (const std::string &key) const;
            void setAuxItem (const std::string &key, const std::string &value);
            void setExtItem (const std::string &key, const std::string &value);

            void deserialize (const cxxtools::SerializationInfo & si);
            void serialize (cxxtools::SerializationInfo & si) const;
            std::string toJson() const;
    };

    std::unique_ptr<BasicAsset>
        getBasicAssetFromFtyProto (fty_proto_t *msg);

    std::unique_ptr<ExtendedAsset>
        getExtendedAssetFromFtyProto (fty_proto_t *msg);

    std::unique_ptr<FullAsset>
        getFullAssetFromFtyProto (fty_proto_t *msg);
}
// end namespace

    void operator>>= (const cxxtools::SerializationInfo & si, fty::BasicAsset & asset);

    void operator>>= (const cxxtools::SerializationInfo & si, fty::ExtendedAsset & asset);

    void operator>>= (const cxxtools::SerializationInfo & si, fty::FullAsset & asset);

    void operator<<= (cxxtools::SerializationInfo & si, const fty::BasicAsset & asset);

    void operator<<= (cxxtools::SerializationInfo & si, const fty::ExtendedAsset & asset);

    void operator<<= (cxxtools::SerializationInfo & si, const fty::FullAsset & asset);

void fty_common_asset_test(bool verbose);
#endif
