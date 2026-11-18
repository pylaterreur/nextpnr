/*
 *  nextpnr -- Next Generation Place and Route
 *
 *  Copyright (C) 2026  nextpnr contributors
 *
 *  Permission to use, copy, modify, and/or distribute this software for any
 *  purpose with or without fee is hereby granted, provided that the above
 *  copyright notice and this permission notice appear in all copies.
 *
 *  THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 *  WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 *  MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 *  ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 *  WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 *  ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 *  OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 *
 */

#include <cstdio>
#include <fstream>
#include <string>
#include "command.h"
#include "gtest/gtest.h"
#include "log.h"
#include "nextpnr.h"
#include "uarch/xilinx/xilinx.h"
#ifdef _WIN32
#include <process.h>
#else
#include <unistd.h>
#endif

USING_NEXTPNR_NAMESPACE

static int process_id()
{
#ifdef _WIN32
    return _getpid();
#else
    return getpid();
#endif
}

// create_clock in the XDC reader, on a Context of the xc7a50t with a net clk to constrain
class XilinxXdcTest : public ::testing::Test
{
  protected:
    void SetUp() override
    {
        init_share_dirname();
        chipArgs.device = "xc7a50tcsg324-1";
        ctx = new Context(chipArgs);
        ctx->uarch->init(ctx);
        ctx->late_init();
        clk = ctx->createNet(ctx->id("clk"));
    }

    void TearDown() override
    {
        std::remove(filename.c_str());
        delete ctx;
    }

    // Reads the constraints from a file, as -o xdc= does
    void parse(const std::string &constraints)
    {
        {
            std::ofstream out(filename);
            out << constraints;
        }
        static_cast<XilinxImpl *>(ctx->uarch.get())->parse_xdc(filename);
    }

    ArchArgs chipArgs;
    Context *ctx;
    NetInfo *clk;
    // one file per process: a test binary of another build may run at the same time
    const std::string filename = ::testing::TempDir() + "nextpnr_xdc_test_" + std::to_string(process_id()) + ".xdc";
};

TEST_F(XilinxXdcTest, a_negative_clock_period_is_an_error)
{
    EXPECT_THROW(parse("create_clock -period -10.000 [get_nets clk]\n"), log_execution_error_exception);
    EXPECT_EQ(clk->clkconstr.get(), nullptr);
}

TEST_F(XilinxXdcTest, a_clock_period_of_zero_is_an_error)
{
    EXPECT_THROW(parse("create_clock -period 0 [get_nets clk]\n"), log_execution_error_exception);
    EXPECT_EQ(clk->clkconstr.get(), nullptr);
}

// std::stod reads "nan" and "inf" as numbers, and converting either to a delay is undefined
TEST_F(XilinxXdcTest, a_clock_period_of_nan_is_an_error)
{
    EXPECT_THROW(parse("create_clock -period nan [get_nets clk]\n"), log_execution_error_exception);
    EXPECT_EQ(clk->clkconstr.get(), nullptr);
}

TEST_F(XilinxXdcTest, an_infinite_clock_period_is_an_error)
{
    EXPECT_THROW(parse("create_clock -period inf [get_nets clk]\n"), log_execution_error_exception);
    EXPECT_EQ(clk->clkconstr.get(), nullptr);
}

TEST_F(XilinxXdcTest, a_positive_clock_period_constrains_the_net)
{
    parse("create_clock -period 10.000 [get_nets clk]\n");
    ASSERT_NE(clk->clkconstr.get(), nullptr);
    EXPECT_EQ(clk->clkconstr->period.minDelay(), 10000);
    EXPECT_EQ(clk->clkconstr->high.minDelay(), 5000);
    EXPECT_EQ(clk->clkconstr->low.minDelay(), 5000);
}
