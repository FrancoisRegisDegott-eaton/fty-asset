#include "test-db.h"
#include "structure/bios_asset_device_type.h"
#include "structure/bios_asset_element.h"
#include "structure/bios_asset_element_type.h"
#include "structure/bios_asset_ext_attributes.h"
#include "structure/bios_asset_group_relation.h"
#include "structure/bios_asset_link.h"
#include "structure/bios_asset_link_type.h"
#include "structure/bios_device_type.h"
#include "structure/bios_discovered_device.h"
#include "structure/bios_monitor_asset_relation.h"
#include "structure/bios_tag.h"
#include <filesystem>
#include <fty_common_db_connection.h>
#include <mariadb/mysql.h>
#include <thread>
#include <iostream>
#include <unistd.h>

namespace fty {

// =====================================================================================================================

class CharArray
{
public:
    template <typename... Args>
    CharArray(const Args&... args)
    {
        add(args...);
        m_data.push_back(nullptr);
    }

    CharArray(const CharArray&) = delete;

    ~CharArray()
    {
        for (size_t i = 0; i < m_data.size(); i++) {
            delete[] m_data[i];
        }
    }

    template <typename... Args>
    void add(const std::string& arg, const Args&... args)
    {
        add(arg);
        add(args...);
    }

    void add(const std::string& str)
    {
        char* s = new char[str.size() + 1];
        memset(s, 0, str.size() + 1);
        strncpy(s, str.c_str(), str.size());
        m_data.push_back(s);
    }

    char** data()
    {
        return m_data.data();
    }

    size_t size() const
    {
        return m_data.size();
    }

private:
    std::vector<char*> m_data;
};

// =====================================================================================================================

static void createDB()
{
    fty::db::Connection conn;
    conn.execute("CREATE DATABASE IF NOT EXISTS box_utf8 character set utf8 collate utf8_general_ci;");
    conn.execute("USE box_utf8");

    createElementType(conn);
    createAssetDeviceType(conn);
    createAssetLinkType(conn);
    createAssetElement(conn);
    createDeviceType(conn);
    createAssetAttributes(conn);
    createGroupRelations(conn);
    createAssetLink(conn);
    createDiscoveryDevice(conn);
    createMonitorAssetRelation(conn);
    createTag(conn);
}

Expected<std::string> TestDb::create()
{
    std::stringstream ss;
    ss << getpid();
    std::string pid = ss.str();

    m_path           = "/tmp/mysql-" + pid;
    std::string sock = "/tmp/mysql-" + pid + ".sock";

    if (!std::filesystem::create_directory(m_path)) {
        return unexpected("cannot create db dir {}", m_path);
    }

    CharArray options("mysql_test", "--datadir=" + m_path, "--socket=" + sock);
    CharArray groups("libmysqld_server", "libmysqld_client");

    mysql_library_init(int(options.size()) - 1, options.data(), groups.data());

    std::string url = "mysql:unix_socket=" + sock;
    setenv("DBURL", url.c_str(), 1);

    try {
        createDB();
    } catch (const std::exception& e) {
        return unexpected(e.what());
    }

    return "mysql:unix_socket=" + sock + ";db=box_utf8";
}

void TestDb::destroy()
{
    if (instance().m_inited) {
        fty::db::shutdown();
        mysql_thread_end();
        mysql_library_end();

        std::filesystem::remove_all(instance().m_path);
    }
}

Expected<void> TestDb::init()
{
    if (instance().m_inited) {
        return {};
    }

    if (auto res = instance().create()) {
        std::string url = *res;
        setenv("DBURL", url.c_str(), 1);
    } else {
        std::cerr << res.error() << std::endl;
        return fty::unexpected(res.error());
    }

    instance().m_inited = true;
    return {};
}

TestDb& TestDb::instance()
{
    static TestDb db;
    return db;
}


} // namespace fty
