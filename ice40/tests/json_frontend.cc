/*
 *  nextpnr -- Next Generation Place and Route
 *
 *  Copyright (C) 2026  Pierre-Yves Le Borgne
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

#include "json_frontend.h"
#include <sstream>
#include "gtest/gtest.h"
#include "nextpnr.h"

USING_NEXTPNR_NAMESPACE

class JsonFrontendTest : public ::testing::Test
{
  protected:
    virtual void SetUp()
    {
        chipArgs.type = ArchArgs::HX1K;
        chipArgs.package = "tq144";
        ctx = new Context(chipArgs);
    }

    virtual void TearDown() { delete ctx; }

    bool load(const std::string &json)
    {
        std::istringstream in(json);
        return parse_json(in, "test.json", ctx);
    }

    ArchArgs chipArgs;
    Context *ctx = nullptr;
};

TEST_F(JsonFrontendTest, two_ports_of_a_submodule_on_one_net_are_connected_to_one_parent_net)
{
    // y and z are the same net inside "sub", and both are connected to the output o of the top module
    ASSERT_TRUE(load(R"({"modules": {
        "top": {"attributes": {"top": "00000000000000000000000000000001"},
                "ports": {"i": {"direction": "input", "bits": [2]}, "o": {"direction": "output", "bits": [3]}},
                "cells": {"u": {"type": "sub", "port_directions": {"a": "input", "y": "output", "z": "output"},
                                "connections": {"a": [2], "y": [3], "z": [3]}}},
                "netnames": {"i": {"bits": [2]}, "o": {"bits": [3]}}},
        "sub": {"ports": {"a": {"direction": "input", "bits": [2]}, "y": {"direction": "output", "bits": [3]},
                          "z": {"direction": "output", "bits": [3]}},
                "cells": {}, "netnames": {"a": {"bits": [2]}, "y": {"bits": [3]}}}
    }})"));
    ctx->check();
    ASSERT_TRUE(ctx->cells.count(ctx->id("o")));
    NetInfo *net = ctx->cells.at(ctx->id("o"))->ports.at(ctx->id("I")).net;
    ASSERT_NE(net, nullptr);
    ASSERT_TRUE(ctx->nets.count(net->name));
    EXPECT_EQ(ctx->nets.at(net->name).get(), net);
}
