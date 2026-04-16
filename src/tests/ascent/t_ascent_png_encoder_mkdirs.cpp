//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~//
// Copyright (c) Lawrence Livermore National Security, LLC and other Ascent
// Project developers. See top-level LICENSE AND COPYRIGHT files for dates and
// other details. No copyright assignment is required to contribute to Ascent.
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~//

//-----------------------------------------------------------------------------
///
/// file: t_ascent_png_encoder_mkdirs.cpp
///
/// Regression test for PNGEncoder::Save creating missing parent directories.
/// Prior behavior was a silent no-op when the output directory didn't exist.
///
//-----------------------------------------------------------------------------

#include "gtest/gtest.h"

#include <png_utils/ascent_png_encoder.hpp>

#include <conduit.hpp>

#include <sys/stat.h>
#include <unistd.h>
#include <vector>
#include <string>

#include "t_config.hpp"
#include "t_utils.hpp"

using namespace std;
using namespace conduit;
using namespace ascent;

//-----------------------------------------------------------------------------
// Produce a small valid RGBA buffer so Encode() has something to work on.
static std::vector<unsigned char>
make_rgba(int w, int h)
{
    std::vector<unsigned char> buf(w * h * 4);
    for(int i = 0; i < w * h; ++i)
    {
        buf[i*4+0] = (unsigned char)(i & 0xff);
        buf[i*4+1] = 0;
        buf[i*4+2] = (unsigned char)((i * 3) & 0xff);
        buf[i*4+3] = 255;
    }
    return buf;
}

//-----------------------------------------------------------------------------
// Save to a path whose parent directory exists. Baseline behavior.
TEST(ascent_png_encoder_mkdirs, save_into_existing_dir)
{
    const std::string base = prepare_output_dir();
    const std::string out = conduit::utils::join_file_path(
        base, "tout_png_mkdirs_existing.png");

    if(conduit::utils::is_file(out))
        conduit::utils::remove_file(out);

    auto rgba = make_rgba(8, 8);
    PNGEncoder enc;
    enc.Encode(rgba.data(), 8, 8);
    enc.Save(out);

    EXPECT_TRUE(conduit::utils::is_file(out))
        << "expected PNG at " << out;
}

//-----------------------------------------------------------------------------
// The regression case: parent directory does NOT exist.
// Prior to the fix, Save() silently failed and no file was created.
TEST(ascent_png_encoder_mkdirs, save_creates_missing_parent)
{
    const std::string base = prepare_output_dir();
    const std::string subdir = conduit::utils::join_file_path(
        base, "tout_png_mkdirs_new_parent");
    const std::string out = conduit::utils::join_file_path(
        subdir, "image.png");

    // Clean slate: remove file and dir if present from a prior run.
    if(conduit::utils::is_file(out))
        conduit::utils::remove_file(out);
    if(conduit::utils::is_directory(subdir))
        conduit::utils::remove_directory(subdir);

    ASSERT_FALSE(conduit::utils::is_directory(subdir))
        << "precondition: parent dir should not exist";

    auto rgba = make_rgba(8, 8);
    PNGEncoder enc;
    enc.Encode(rgba.data(), 8, 8);
    enc.Save(out);

    EXPECT_TRUE(conduit::utils::is_directory(subdir))
        << "parent dir should have been created: " << subdir;
    EXPECT_TRUE(conduit::utils::is_file(out))
        << "expected PNG at " << out;
}

//-----------------------------------------------------------------------------
// Nested-missing case: multiple levels of parent don't exist.
// Covers the mkdir -p equivalent walk.
TEST(ascent_png_encoder_mkdirs, save_creates_nested_missing_parents)
{
    const std::string base = prepare_output_dir();
    const std::string lvl1 = conduit::utils::join_file_path(
        base, "tout_png_mkdirs_nested");
    const std::string lvl2 = conduit::utils::join_file_path(lvl1, "a");
    const std::string lvl3 = conduit::utils::join_file_path(lvl2, "b");
    const std::string lvl4 = conduit::utils::join_file_path(lvl3, "c");
    const std::string out  = conduit::utils::join_file_path(lvl4, "image.png");

    // Tear down in reverse order if a prior run left anything.
    if(conduit::utils::is_file(out))
        conduit::utils::remove_file(out);
    if(conduit::utils::is_directory(lvl4))
        conduit::utils::remove_directory(lvl4);
    if(conduit::utils::is_directory(lvl3))
        conduit::utils::remove_directory(lvl3);
    if(conduit::utils::is_directory(lvl2))
        conduit::utils::remove_directory(lvl2);
    if(conduit::utils::is_directory(lvl1))
        conduit::utils::remove_directory(lvl1);

    ASSERT_FALSE(conduit::utils::is_directory(lvl1))
        << "precondition: top-level parent should not exist";

    auto rgba = make_rgba(8, 8);
    PNGEncoder enc;
    enc.Encode(rgba.data(), 8, 8);
    enc.Save(out);

    EXPECT_TRUE(conduit::utils::is_directory(lvl1));
    EXPECT_TRUE(conduit::utils::is_directory(lvl2));
    EXPECT_TRUE(conduit::utils::is_directory(lvl3));
    EXPECT_TRUE(conduit::utils::is_directory(lvl4));
    EXPECT_TRUE(conduit::utils::is_file(out));
}

//-----------------------------------------------------------------------------
// Relative path with no directory component should just work.
TEST(ascent_png_encoder_mkdirs, save_bare_filename)
{
    // Use the test output dir as cwd-equivalent so we don't litter.
    const std::string base = prepare_output_dir();
    const std::string out = conduit::utils::join_file_path(
        base, "tout_png_mkdirs_bare.png");

    if(conduit::utils::is_file(out))
        conduit::utils::remove_file(out);

    auto rgba = make_rgba(8, 8);
    PNGEncoder enc;
    enc.Encode(rgba.data(), 8, 8);
    enc.Save(out);

    EXPECT_TRUE(conduit::utils::is_file(out));
}

//-----------------------------------------------------------------------------
// Read-only parent: mkdir() should fail with EACCES (not EEXIST), and
// Save() should surface the failure via CONDUIT_ERROR -> conduit::Error.
// Skipped when running as root, since root ignores directory permissions.
TEST(ascent_png_encoder_mkdirs, save_fails_on_readonly_parent)
{
    if(geteuid() == 0)
    {
        GTEST_SKIP() << "running as root; permission checks bypassed";
    }

    const std::string base = prepare_output_dir();
    const std::string ro_parent = conduit::utils::join_file_path(
        base, "tout_png_mkdirs_ro_parent");
    const std::string new_child = conduit::utils::join_file_path(
        ro_parent, "child");
    const std::string out = conduit::utils::join_file_path(
        new_child, "image.png");

    // Tear down any leftover state, restoring perms first so rmdir can
    // succeed if a prior run aborted mid-test.
    if(conduit::utils::is_directory(ro_parent))
    {
        chmod(ro_parent.c_str(), 0755);
        if(conduit::utils::is_directory(new_child))
            conduit::utils::remove_directory(new_child);
        conduit::utils::remove_directory(ro_parent);
    }

    // Create parent, then strip write permission.
    ASSERT_TRUE(conduit::utils::create_directory(ro_parent));
    ASSERT_EQ(0, chmod(ro_parent.c_str(), 0555))
        << "failed to set read-only perms on " << ro_parent;

    auto rgba = make_rgba(8, 8);
    PNGEncoder enc;
    enc.Encode(rgba.data(), 8, 8);

    EXPECT_THROW(enc.Save(out), conduit::Error)
        << "Save into read-only parent should have thrown";
    EXPECT_FALSE(conduit::utils::is_file(out));
    EXPECT_FALSE(conduit::utils::is_directory(new_child));

    // Restore perms so the test output dir can be cleaned up later.
    chmod(ro_parent.c_str(), 0755);
    conduit::utils::remove_directory(ro_parent);
}

//-----------------------------------------------------------------------------
int main(int argc, char *argv[])
{
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~//
// Copyright (c) Lawrence Livermore National Security, LLC and other Ascent
// Project developers. See top-level LICENSE AND COPYRIGHT files for dates and
// other details. No copyright assignment is required to contribute to Ascent.
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~//

//-----------------------------------------------------------------------------
///
/// file: t_ascent_png_encoder_mkdirs.cpp
///
/// Regression test for PNGEncoder::Save creating missing parent directories.
/// Prior behavior was a silent no-op when the output directory didn't exist.
///
//-----------------------------------------------------------------------------

#include "gtest/gtest.h"

#include <png_utils/ascent_png_encoder.hpp>

#include <conduit.hpp>

#include <sys/stat.h>
#include <unistd.h>
#include <vector>
#include <string>

#include "t_config.hpp"
#include "t_utils.hpp"

using namespace std;
using namespace conduit;
using namespace ascent;

//-----------------------------------------------------------------------------
// Produce a small valid RGBA buffer so Encode() has something to work on.
static std::vector<unsigned char>
make_rgba(int w, int h)
{
    std::vector<unsigned char> buf(w * h * 4);
    for(int i = 0; i < w * h; ++i)
    {
        buf[i*4+0] = (unsigned char)(i & 0xff);
        buf[i*4+1] = 0;
        buf[i*4+2] = (unsigned char)((i * 3) & 0xff);
        buf[i*4+3] = 255;
    }
    return buf;
}

//-----------------------------------------------------------------------------
// Save to a path whose parent directory exists. Baseline behavior.
TEST(ascent_png_encoder_mkdirs, save_into_existing_dir)
{
    const std::string base = prepare_output_dir();
    const std::string out = conduit::utils::join_file_path(
        base, "tout_png_mkdirs_existing.png");

    if(conduit::utils::is_file(out))
        conduit::utils::remove_file(out);

    auto rgba = make_rgba(8, 8);
    PNGEncoder enc;
    enc.Encode(rgba.data(), 8, 8);
    enc.Save(out);

    EXPECT_TRUE(conduit::utils::is_file(out))
        << "expected PNG at " << out;
}

//-----------------------------------------------------------------------------
// The regression case: parent directory does NOT exist.
// Prior to the fix, Save() silently failed and no file was created.
TEST(ascent_png_encoder_mkdirs, save_creates_missing_parent)
{
    const std::string base = prepare_output_dir();
    const std::string subdir = conduit::utils::join_file_path(
        base, "tout_png_mkdirs_new_parent");
    const std::string out = conduit::utils::join_file_path(
        subdir, "image.png");

    // Clean slate: remove file and dir if present from a prior run.
    if(conduit::utils::is_file(out))
        conduit::utils::remove_file(out);
    if(conduit::utils::is_directory(subdir))
        conduit::utils::remove_directory(subdir);

    ASSERT_FALSE(conduit::utils::is_directory(subdir))
        << "precondition: parent dir should not exist";

    auto rgba = make_rgba(8, 8);
    PNGEncoder enc;
    enc.Encode(rgba.data(), 8, 8);
    enc.Save(out);

    EXPECT_TRUE(conduit::utils::is_directory(subdir))
        << "parent dir should have been created: " << subdir;
    EXPECT_TRUE(conduit::utils::is_file(out))
        << "expected PNG at " << out;
}

//-----------------------------------------------------------------------------
// Nested-missing case: multiple levels of parent don't exist.
// Covers the mkdir -p equivalent walk.
TEST(ascent_png_encoder_mkdirs, save_creates_nested_missing_parents)
{
    const std::string base = prepare_output_dir();
    const std::string lvl1 = conduit::utils::join_file_path(
        base, "tout_png_mkdirs_nested");
    const std::string lvl2 = conduit::utils::join_file_path(lvl1, "a");
    const std::string lvl3 = conduit::utils::join_file_path(lvl2, "b");
    const std::string lvl4 = conduit::utils::join_file_path(lvl3, "c");
    const std::string out  = conduit::utils::join_file_path(lvl4, "image.png");

    // Tear down in reverse order if a prior run left anything.
    if(conduit::utils::is_file(out))
        conduit::utils::remove_file(out);
    if(conduit::utils::is_directory(lvl4))
        conduit::utils::remove_directory(lvl4);
    if(conduit::utils::is_directory(lvl3))
        conduit::utils::remove_directory(lvl3);
    if(conduit::utils::is_directory(lvl2))
        conduit::utils::remove_directory(lvl2);
    if(conduit::utils::is_directory(lvl1))
        conduit::utils::remove_directory(lvl1);

    ASSERT_FALSE(conduit::utils::is_directory(lvl1))
        << "precondition: top-level parent should not exist";

    auto rgba = make_rgba(8, 8);
    PNGEncoder enc;
    enc.Encode(rgba.data(), 8, 8);
    enc.Save(out);

    EXPECT_TRUE(conduit::utils::is_directory(lvl1));
    EXPECT_TRUE(conduit::utils::is_directory(lvl2));
    EXPECT_TRUE(conduit::utils::is_directory(lvl3));
    EXPECT_TRUE(conduit::utils::is_directory(lvl4));
    EXPECT_TRUE(conduit::utils::is_file(out));
}

//-----------------------------------------------------------------------------
// Relative path with no directory component should just work.
TEST(ascent_png_encoder_mkdirs, save_bare_filename)
{
    // Use the test output dir as cwd-equivalent so we don't litter.
    const std::string base = prepare_output_dir();
    const std::string out = conduit::utils::join_file_path(
        base, "tout_png_mkdirs_bare.png");

    if(conduit::utils::is_file(out))
        conduit::utils::remove_file(out);

    auto rgba = make_rgba(8, 8);
    PNGEncoder enc;
    enc.Encode(rgba.data(), 8, 8);
    enc.Save(out);

    EXPECT_TRUE(conduit::utils::is_file(out));
}

//-----------------------------------------------------------------------------
// Read-only parent: mkdir() should fail with EACCES (not EEXIST), and
// Save() should surface the failure via CONDUIT_ERROR -> conduit::Error.
// Skipped when running as root, since root ignores directory permissions.
TEST(ascent_png_encoder_mkdirs, save_fails_on_readonly_parent)
{
    if(geteuid() == 0)
    {
        GTEST_SKIP() << "running as root; permission checks bypassed";
    }

    const std::string base = prepare_output_dir();
    const std::string ro_parent = conduit::utils::join_file_path(
        base, "tout_png_mkdirs_ro_parent");
    const std::string new_child = conduit::utils::join_file_path(
        ro_parent, "child");
    const std::string out = conduit::utils::join_file_path(
        new_child, "image.png");

    // Tear down any leftover state, restoring perms first so rmdir can
    // succeed if a prior run aborted mid-test.
    if(conduit::utils::is_directory(ro_parent))
    {
        chmod(ro_parent.c_str(), 0755);
        if(conduit::utils::is_directory(new_child))
            conduit::utils::remove_directory(new_child);
        conduit::utils::remove_directory(ro_parent);
    }

    // Create parent, then strip write permission.
    ASSERT_TRUE(conduit::utils::create_directory(ro_parent));
    ASSERT_EQ(0, chmod(ro_parent.c_str(), 0555))
        << "failed to set read-only perms on " << ro_parent;

    auto rgba = make_rgba(8, 8);
    PNGEncoder enc;
    enc.Encode(rgba.data(), 8, 8);

    EXPECT_THROW(enc.Save(out), conduit::Error)
        << "Save into read-only parent should have thrown";
    EXPECT_FALSE(conduit::utils::is_file(out));
    EXPECT_FALSE(conduit::utils::is_directory(new_child));

    // Restore perms so the test output dir can be cleaned up later.
    chmod(ro_parent.c_str(), 0755);
    conduit::utils::remove_directory(ro_parent);
}

//-----------------------------------------------------------------------------
int main(int argc, char *argv[])
{
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~//
// Copyright (c) Lawrence Livermore National Security, LLC and other Ascent
// Project developers. See top-level LICENSE AND COPYRIGHT files for dates and
// other details. No copyright assignment is required to contribute to Ascent.
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~//

//-----------------------------------------------------------------------------
///
/// file: t_ascent_png_encoder_mkdirs.cpp
///
/// Regression test for PNGEncoder::Save creating missing parent directories.
/// Prior behavior was a silent no-op when the output directory didn't exist.
///
//-----------------------------------------------------------------------------

#include "gtest/gtest.h"

#include <png_utils/ascent_png_encoder.hpp>

#include <conduit.hpp>

#include <sys/stat.h>
#include <unistd.h>
#include <vector>
#include <string>

#include "t_config.hpp"
#include "t_utils.hpp"

using namespace std;
using namespace conduit;
using namespace ascent;

//-----------------------------------------------------------------------------
// Produce a small valid RGBA buffer so Encode() has something to work on.
static std::vector<unsigned char>
make_rgba(int w, int h)
{
    std::vector<unsigned char> buf(w * h * 4);
    for(int i = 0; i < w * h; ++i)
    {
        buf[i*4+0] = (unsigned char)(i & 0xff);
        buf[i*4+1] = 0;
        buf[i*4+2] = (unsigned char)((i * 3) & 0xff);
        buf[i*4+3] = 255;
    }
    return buf;
}

//-----------------------------------------------------------------------------
// Save to a path whose parent directory exists. Baseline behavior.
TEST(ascent_png_encoder_mkdirs, save_into_existing_dir)
{
    const std::string base = prepare_output_dir();
    const std::string out = conduit::utils::join_file_path(
        base, "tout_png_mkdirs_existing.png");

    if(conduit::utils::is_file(out))
        conduit::utils::remove_file(out);

    auto rgba = make_rgba(8, 8);
    PNGEncoder enc;
    enc.Encode(rgba.data(), 8, 8);
    enc.Save(out);

    EXPECT_TRUE(conduit::utils::is_file(out))
        << "expected PNG at " << out;
}

//-----------------------------------------------------------------------------
// The regression case: parent directory does NOT exist.
// Prior to the fix, Save() silently failed and no file was created.
TEST(ascent_png_encoder_mkdirs, save_creates_missing_parent)
{
    const std::string base = prepare_output_dir();
    const std::string subdir = conduit::utils::join_file_path(
        base, "tout_png_mkdirs_new_parent");
    const std::string out = conduit::utils::join_file_path(
        subdir, "image.png");

    // Clean slate: remove file and dir if present from a prior run.
    if(conduit::utils::is_file(out))
        conduit::utils::remove_file(out);
    if(conduit::utils::is_directory(subdir))
        conduit::utils::remove_directory(subdir);

    ASSERT_FALSE(conduit::utils::is_directory(subdir))
        << "precondition: parent dir should not exist";

    auto rgba = make_rgba(8, 8);
    PNGEncoder enc;
    enc.Encode(rgba.data(), 8, 8);
    enc.Save(out);

    EXPECT_TRUE(conduit::utils::is_directory(subdir))
        << "parent dir should have been created: " << subdir;
    EXPECT_TRUE(conduit::utils::is_file(out))
        << "expected PNG at " << out;
}

//-----------------------------------------------------------------------------
// Nested-missing case: multiple levels of parent don't exist.
// Covers the mkdir -p equivalent walk.
TEST(ascent_png_encoder_mkdirs, save_creates_nested_missing_parents)
{
    const std::string base = prepare_output_dir();
    const std::string lvl1 = conduit::utils::join_file_path(
        base, "tout_png_mkdirs_nested");
    const std::string lvl2 = conduit::utils::join_file_path(lvl1, "a");
    const std::string lvl3 = conduit::utils::join_file_path(lvl2, "b");
    const std::string lvl4 = conduit::utils::join_file_path(lvl3, "c");
    const std::string out  = conduit::utils::join_file_path(lvl4, "image.png");

    // Tear down in reverse order if a prior run left anything.
    if(conduit::utils::is_file(out))
        conduit::utils::remove_file(out);
    if(conduit::utils::is_directory(lvl4))
        conduit::utils::remove_directory(lvl4);
    if(conduit::utils::is_directory(lvl3))
        conduit::utils::remove_directory(lvl3);
    if(conduit::utils::is_directory(lvl2))
        conduit::utils::remove_directory(lvl2);
    if(conduit::utils::is_directory(lvl1))
        conduit::utils::remove_directory(lvl1);

    ASSERT_FALSE(conduit::utils::is_directory(lvl1))
        << "precondition: top-level parent should not exist";

    auto rgba = make_rgba(8, 8);
    PNGEncoder enc;
    enc.Encode(rgba.data(), 8, 8);
    enc.Save(out);

    EXPECT_TRUE(conduit::utils::is_directory(lvl1));
    EXPECT_TRUE(conduit::utils::is_directory(lvl2));
    EXPECT_TRUE(conduit::utils::is_directory(lvl3));
    EXPECT_TRUE(conduit::utils::is_directory(lvl4));
    EXPECT_TRUE(conduit::utils::is_file(out));
}

//-----------------------------------------------------------------------------
// Relative path with no directory component should just work.
TEST(ascent_png_encoder_mkdirs, save_bare_filename)
{
    // Use the test output dir as cwd-equivalent so we don't litter.
    const std::string base = prepare_output_dir();
    const std::string out = conduit::utils::join_file_path(
        base, "tout_png_mkdirs_bare.png");

    if(conduit::utils::is_file(out))
        conduit::utils::remove_file(out);

    auto rgba = make_rgba(8, 8);
    PNGEncoder enc;
    enc.Encode(rgba.data(), 8, 8);
    enc.Save(out);

    EXPECT_TRUE(conduit::utils::is_file(out));
}

//-----------------------------------------------------------------------------
// Read-only parent: mkdir() should fail with EACCES (not EEXIST), and
// Save() should surface the failure via CONDUIT_ERROR -> conduit::Error.
// Skipped when running as root, since root ignores directory permissions.
TEST(ascent_png_encoder_mkdirs, save_fails_on_readonly_parent)
{
    if(geteuid() == 0)
    {
        GTEST_SKIP() << "running as root; permission checks bypassed";
    }

    const std::string base = prepare_output_dir();
    const std::string ro_parent = conduit::utils::join_file_path(
        base, "tout_png_mkdirs_ro_parent");
    const std::string new_child = conduit::utils::join_file_path(
        ro_parent, "child");
    const std::string out = conduit::utils::join_file_path(
        new_child, "image.png");

    // Tear down any leftover state, restoring perms first so rmdir can
    // succeed if a prior run aborted mid-test.
    if(conduit::utils::is_directory(ro_parent))
    {
        chmod(ro_parent.c_str(), 0755);
        if(conduit::utils::is_directory(new_child))
            conduit::utils::remove_directory(new_child);
        conduit::utils::remove_directory(ro_parent);
    }

    // Create parent, then strip write permission.
    ASSERT_TRUE(conduit::utils::create_directory(ro_parent));
    ASSERT_EQ(0, chmod(ro_parent.c_str(), 0555))
        << "failed to set read-only perms on " << ro_parent;

    auto rgba = make_rgba(8, 8);
    PNGEncoder enc;
    enc.Encode(rgba.data(), 8, 8);

    EXPECT_THROW(enc.Save(out), conduit::Error)
        << "Save into read-only parent should have thrown";
    EXPECT_FALSE(conduit::utils::is_file(out));
    EXPECT_FALSE(conduit::utils::is_directory(new_child));

    // Restore perms so the test output dir can be cleaned up later.
    chmod(ro_parent.c_str(), 0755);
    conduit::utils::remove_directory(ro_parent);
}

//-----------------------------------------------------------------------------
int main(int argc, char *argv[])
{
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
