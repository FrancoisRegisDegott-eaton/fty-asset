/*  ========================================================================
    Copyright (C) 2020 Eaton
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
    ========================================================================
*/

//#define CATCH_CONFIG_MAIN
#define CATCH_CONFIG_RUNNER //own main()
#include <catch2/catch.hpp>

//#include <fty_log.h>
#include "asset/asset.h"

static int runCatch2Tests( int argc, char* argv[] )
{
    Catch::Session session; // There must be exactly one instance

    // writing to session.configData() here sets defaults
    // this is the preferred way to set them

    int returnCode = session.applyCommandLine( argc, argv );
    if( returnCode != 0 ) // Indicates a command line error
        return returnCode;

    // writing to session.configData() or session.Config() here
    // overrides command line args
    // only do this if you know you need to

    int numFailed = session.run();

    // numFailed is clamped to 255 as some unices only use the lower 8 bits.
    // This clamping has already been applied, so just return it here
    // You can also do any post run clean-up here
    return numFailed;
}

int main( int argc, char* argv[] )
{
    //bool verbose = true;
    //ManageFtyLog::setInstanceFtylog("fty-asset-test", FTY_COMMON_LOGGING_DEFAULT_CFG);
    //if (verbose) {
    //    ManageFtyLog::getInstanceFtylog()->setVerboseMode();
    //}

    g_testMode = true;

    return runCatch2Tests(argc, argv);
}
