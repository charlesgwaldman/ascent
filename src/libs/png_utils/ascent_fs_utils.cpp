//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~//
// Copyright (c) Lawrence Livermore National Security, LLC and other Ascent
// Project developers. See top-level LICENSE AND COPYRIGHT files for dates and
// other details. No copyright assignment is required to contribute to Ascent.
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~//

//-----------------------------------------------------------------------------
///
/// file: ascent_fs_utils.cpp
///
//-----------------------------------------------------------------------------

#include "ascent_fs_utils.hpp"

#include <sys/stat.h>
#include <errno.h>
#include <string.h>
#include <vector>

#include <conduit.hpp>

//-----------------------------------------------------------------------------
namespace ascent
{

//-----------------------------------------------------------------------------
void
mkdirs(const std::string &filepath)
{
    std::string::size_type pos = filepath.rfind('/');
    if(pos == std::string::npos || pos == 0)
        return;

    std::string dir = filepath.substr(0, pos);

    struct stat st;
    if(stat(dir.c_str(), &st) == 0)
        return;

    // Collect path components that need creating, walking up to the
    // deepest existing ancestor.
    std::vector<std::string> to_create;
    std::string d = dir;
    while(!d.empty())
    {
        if(stat(d.c_str(), &st) == 0)
            break;
        to_create.push_back(d);
        std::string::size_type p = d.rfind('/');
        if(p == std::string::npos)
            break;
        d = d.substr(0, p);
    }

    // Create from shallowest to deepest. EEXIST is benign (race-safe).
    for(auto it = to_create.rbegin(); it != to_create.rend(); ++it)
    {
        if(mkdir(it->c_str(), 0755) != 0 && errno != EEXIST)
        {
            CONDUIT_ERROR("Failed to create directory '"
                          << *it << "': " << strerror(errno));
        }
    }
}

}
//-----------------------------------------------------------------------------
// -- end ascent:: --
//-----------------------------------------------------------------------------
