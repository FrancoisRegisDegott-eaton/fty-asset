#pragma once

#include <pack/pack.h>

namespace fty::asset {

struct LinkEntry : public pack::Node
{
    pack::String source    = FIELD("source");
    pack::Int32  link_type = FIELD("link_type");
    pack::String src_out   = FIELD("src_out");

    using pack::Node::Node;
    META(LinkEntry, source, link_type, src_out);
};

struct ExtEntry : public pack::Node
{
    pack::String value    = FIELD("value");
    pack::Bool   readOnly = FIELD("readOnly");
    pack::Bool   update   = FIELD("update");

    using pack::Node::Node;
    META(ExtEntry, value, readOnly, update);
};

struct Dto : public pack::Node
{
    // enum class Status
    // {
    //     Unknown,
    //     Active,
    //     Nonactive
    // };

    pack::UInt32 status   = FIELD("status");
    pack::String type     = FIELD("type");
    pack::String sub_type = FIELD("sub_type");
    pack::String name     = FIELD("name");
    pack::Int32  priority = FIELD("priority");
    pack::String parent   = FIELD("parent");

    pack::ObjectList<LinkEntry> linked = FIELD("linked");

    pack::Map<ExtEntry> ext = FIELD("ext");

    using pack::Node::Node;
    META(Dto, status, type, sub_type, name, priority, parent, linked, ext);
}; // namespace asset

} // namespace fty::asset
