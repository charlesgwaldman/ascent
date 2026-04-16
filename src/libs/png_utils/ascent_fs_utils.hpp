//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~//
// Copyright (c) Lawrence Livermore National Security, LLC and other Ascent
// Project developers. See top-level LICENSE AND COPYRIGHT files for dates and
// other details. No copyright assignment is required to contribute to Ascent.
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~//

//-----------------------------------------------------------------------------
///
/// file: ascent_fs_utils.hpp
///
/// Small filesystem helpers used by png_utils and its consumers.
///
//-----------------------------------------------------------------------------

#ifndef ASCENT_FS_UTILS_HPP
#define ASCENT_FS_UTILS_HPP

#include "ascent_png_utils_exports.h"

#include <string>

//-----------------------------------------------------------------------------
namespace ascent
{

//-----------------------------------------------------------------------------
/// Ensure all directories in the parent path of `filepath` exist, creating
/// any missing components (equivalent to `mkdir -p` on the parent).
///
/// Uses POSIX mkdir(). Races between concurrent writers (e.g. MPI ranks
/// all saving into the same directory) are harmless: EEXIST is treated as
/// success. Any other mkdir() failure is reported via CONDUIT_ERROR, which
/// throws conduit::Error.
///
/// No-op when `filepath` has no directory component.
//-----------------------------------------------------------------------------
void ASCENT_API mkdirs(const std::string &filepath);

}
//-----------------------------------------------------------------------------
// -- end ascent:: --
//-----------------------------------------------------------------------------

#endif
