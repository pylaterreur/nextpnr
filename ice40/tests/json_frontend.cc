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
#include "log.h"
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

// Importing a module that instantiates itself recurses until the stack is exhausted
TEST_F(JsonFrontendTest, a_module_that_instantiates_itself_is_an_error)
{
    std::string logged;
    log_write_function = [&](std::string text) { logged += text; };
    EXPECT_THROW(load(R"({"modules": {
        "top": {"attributes": {"top": "00000000000000000000000000000001"}, "ports": {},
                "cells": {"u": {"type": "sub", "port_directions": {}, "connections": {}}}, "netnames": {}},
        "sub": {"ports": {}, "cells": {"v": {"type": "sub", "port_directions": {}, "connections": {}}}, "netnames": {}}
    }})"),
                 log_execution_error_exception);
    log_write_function = nullptr;
    EXPECT_NE(logged.find("Module 'sub' instantiates itself (cell 'v' in module 'sub')"), std::string::npos) << logged;
}

TEST_F(JsonFrontendTest, a_cycle_of_modules_is_an_error)
{
    // top contains an "a", which contains a "b", which contains an "a" again
    EXPECT_THROW(load(R"({"modules": {
        "top": {"attributes": {"top": "00000000000000000000000000000001"}, "ports": {},
                "cells": {"u": {"type": "a", "port_directions": {}, "connections": {}}}, "netnames": {}},
        "a": {"ports": {}, "cells": {"x": {"type": "b", "port_directions": {}, "connections": {}}}, "netnames": {}},
        "b": {"ports": {}, "cells": {"y": {"type": "a", "port_directions": {}, "connections": {}}}, "netnames": {}}
    }})"),
                 log_execution_error_exception);
}

TEST_F(JsonFrontendTest, a_cycle_at_the_end_of_a_very_deep_hierarchy_is_an_error)
{
    // m0 contains m1 and so on, and the last module contains itself: the check has to reach the end of the chain
    const int depth = 60000;
    std::string netlist = "{\"modules\": {\n";
    for (int i = 0; i <= depth; i++) {
        const std::string next = "m" + std::to_string(i < depth ? i + 1 : depth);
        netlist += "\"m" + std::to_string(i) + "\": {" +
                   (i == 0 ? "\"attributes\": {\"top\": \"00000000000000000000000000000001\"}, " : "") +
                   "\"ports\": {}, \"netnames\": {}, \"cells\": {\"c\": {\"type\": \"" + next +
                   "\", \"port_directions\": {}, \"connections\": {}}}}" + (i < depth ? ",\n" : "\n");
    }
    netlist += "}}";
    EXPECT_THROW(load(netlist), log_execution_error_exception);
}

TEST_F(JsonFrontendTest, a_recursive_module_that_the_design_does_not_reach_is_not_an_error)
{
    // nothing below top uses "orphan", so it is never imported
    ASSERT_TRUE(load(R"({"modules": {
        "top": {"attributes": {"top": "00000000000000000000000000000001"}, "ports": {},
                "cells": {"u": {"type": "sub", "port_directions": {}, "connections": {}}}, "netnames": {}},
        "sub": {"ports": {}, "cells": {}, "netnames": {}},
        "orphan": {"ports": {}, "netnames": {},
                   "cells": {"o": {"type": "orphan", "port_directions": {}, "connections": {}}}}
    }})"));
    EXPECT_TRUE(ctx->hierarchy.count(ctx->id("top/u")));
    EXPECT_FALSE(ctx->hierarchy.count(ctx->id("orphan")));
}

TEST_F(JsonFrontendTest, a_module_instantiated_more_than_once_is_not_recursion)
{
    // "sub" is used by two siblings, and each copy contains a "leaf"
    ASSERT_TRUE(load(R"({"modules": {
        "top": {"attributes": {"top": "00000000000000000000000000000001"}, "ports": {},
                "cells": {"u1": {"type": "sub", "port_directions": {}, "connections": {}},
                          "u2": {"type": "sub", "port_directions": {}, "connections": {}}}, "netnames": {}},
        "sub": {"ports": {}, "cells": {"w": {"type": "leaf", "port_directions": {}, "connections": {}}}, "netnames": {}},
        "leaf": {"ports": {}, "cells": {}, "netnames": {}}
    }})"));
    for (const char *path : {"top", "top/u1", "top/u1/w", "top/u2", "top/u2/w"})
        EXPECT_TRUE(ctx->hierarchy.count(ctx->id(path))) << path;
}

TEST_F(JsonFrontendTest, a_module_with_very_many_instances_of_one_module_loads)
{
    const int instances = 200000;
    std::string cells;
    for (int i = 0; i < instances; i++)
        cells += std::string(i ? "," : "") + "\"u" + std::to_string(i) +
                 "\": {\"type\": \"sub\", \"port_directions\": {}, \"connections\": {}}";
    ASSERT_TRUE(load(R"({"modules": {
        "top": {"attributes": {"top": "00000000000000000000000000000001"}, "ports": {}, "netnames": {},
                "cells": {)" +
                     cells + R"(}},
        "sub": {"ports": {}, "cells": {}, "netnames": {}}
    }})"));
    EXPECT_EQ(ctx->hierarchy.at(ctx->id("top")).hier_cells.size(), size_t(instances));
}
