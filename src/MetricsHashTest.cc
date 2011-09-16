/* Copyright (c) 2011 Stanford University
 *
 * Permission to use, copy, modify, and distribute this software for
 * any purpose with or without fee is hereby granted, provided that
 * the above copyright notice and this permission notice appear in all
 * copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR(S) DISCLAIM ALL
 * WARRANTIES WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL
 * AUTHORS BE LIABLE FOR ANY SPECIAL, DIRECT, INDIRECT, OR
 * CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS
 * OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT,
 * NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN
 * CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include "TestUtil.h"
#include "RawMetrics.h"
#include "MetricsHash.h"
#include "MetricList.pb.h"

namespace RAMCloud {

class MetricsHashTest : public ::testing::Test {
  public:
    MetricsHashTest() { }
    DISALLOW_COPY_AND_ASSIGN(MetricsHashTest);
};

TEST_F(MetricsHashTest, basics) {
    MetricsHash metrics;
    metrics["a.b.c"] = 24;
    metrics["a.b.d"] = 36;
    metrics["a.b.c"] = 48;
    EXPECT_EQ(48U, metrics["a.b.c"]);
    EXPECT_EQ(36U, metrics["a.b.d"]);
}

TEST_F(MetricsHashTest, load) {
    RawMetrics data;
    data.master.recoveryTicks = 99;
    data.backup.writeCount = 1000;
    string s;
    data.serialize(s);
    Buffer buffer;
    Buffer::Chunk::appendToBuffer(&buffer, s.c_str(),
            downCast<uint32_t>(s.length()));
    MetricsHash metrics;
    metrics.load(buffer);
    EXPECT_EQ(99U, metrics["master.recoveryTicks"]);
    EXPECT_EQ(1000U, metrics["backup.writeCount"]);
}

TEST_F(MetricsHashTest, load_bogusInput) {
    string s("This string contains bogus data");
    Buffer buffer;
    Buffer::Chunk::appendToBuffer(&buffer, s.c_str(),
            downCast<uint32_t>(s.length()));
    MetricsHash metrics;
    EXPECT_THROW(metrics.load(buffer), MetricsHash::FormatError);
}

TEST_F(MetricsHashTest, difference) {
    MetricsHash metrics;
    metrics["a"] = 10;
    metrics["b"] = 20;
    metrics["c"] = 30;
    MetricsHash metrics2;
    metrics2["a"] = 1;
    metrics2["b"] = 2;
    metrics2["d"] = 3;
    metrics.difference(metrics2);
    EXPECT_EQ(9U, metrics["a"]);
    EXPECT_EQ(18U, metrics["b"]);
    EXPECT_EQ(30U, metrics["c"]);
    EXPECT_EQ(0xfffffffffffffffdul, metrics["d"]);
}

TEST_F(MetricsHashTest, difference_skipSpecialValues) {
    MetricsHash metrics;
    metrics["clockFrequency"] = 10;
    metrics["pid"] = 20;
    metrics["serverId"] = 30;
    MetricsHash metrics2;
    metrics2["clockFrequency"] = 1;
    metrics2["pid"] = 2;
    metrics2["serverId"] = 3;
    metrics.difference(metrics2);
    EXPECT_EQ(10U, metrics["clockFrequency"]);
    EXPECT_EQ(20U, metrics["pid"]);
    EXPECT_EQ(30U, metrics["serverId"]);
}

TEST_F(MetricsHashTest, difference_vectors) {
    std::vector<MetricsHash> first;
    std::vector<MetricsHash> second;
    first.resize(2);
    first[0]["serverId"] = 10;
    first[0]["x"] = 1;
    first[0]["y"] = 2;
    first[1]["serverId"] = 20;
    first[1]["x"] = 100;
    first[1]["y"] = 200;
    second.resize(4);
    second[0]["serverId"] = 14;
    second[1]["serverId"] = 20;
    second[1]["x"] = 1000;
    second[1]["y"] = 2000;
    second[2]["serverId"] = 99;
    second[3]["serverId"] = 10;
    second[3]["x"] = 50;
    second[3]["y"] = 60;
    EXPECT_EQ(2, MetricsHash::difference(first, second));
    EXPECT_EQ(0U, second[0].size());
    EXPECT_EQ(20U, second[1]["serverId"]);
    EXPECT_EQ(900U, second[1]["x"]);
    EXPECT_EQ(1800U, second[1]["y"]);
    EXPECT_EQ(0U, second[2].size());
    EXPECT_EQ(10U, second[3]["serverId"]);
    EXPECT_EQ(49U, second[3]["x"]);
    EXPECT_EQ(58U, second[3]["y"]);
}

// The following tests are for methods defined in MetricsHash.h.

TEST_F(MetricsHashTest, iteration) {
    MetricsHash metrics;
    metrics["a"] = 1;
    metrics["b"] = 10;
    metrics["c"] = 100;
    uint64_t total = 0;
    for (MetricsHash::iterator it = metrics.begin(); it != metrics.end();
            it++) {
        total += it->second;
    }
    EXPECT_EQ(111U, total);
}

TEST_F(MetricsHashTest, clear) {
    MetricsHash metrics;
    metrics["a"] = 1;
    metrics["b"] = 10;
    metrics["c"] = 100;
    metrics.clear();
    uint64_t total = 0;
    for (MetricsHash::iterator it = metrics.begin(); it != metrics.end();
            it++) {
        total += it->second;
    }
    EXPECT_EQ(0U, total);
}

TEST_F(MetricsHashTest, erase) {
    MetricsHash metrics;
    metrics["a"] = 1;
    metrics["b"] = 10;
    metrics["c"] = 100;
    metrics.erase("b");
    uint64_t total = 0;
    for (MetricsHash::iterator it = metrics.begin(); it != metrics.end();
            it++) {
        total += it->second;
    }
    EXPECT_EQ(101U, total);
}

TEST_F(MetricsHashTest, empty) {
    MetricsHash metrics;
    metrics["a"] = 1;
    metrics["b"] = 10;
    metrics["c"] = 100;
    metrics.erase("b");
    EXPECT_FALSE(metrics.empty());
    metrics.erase("a");
    EXPECT_FALSE(metrics.empty());
    metrics.erase("c");
    EXPECT_TRUE(metrics.empty());
}

TEST_F(MetricsHashTest, size) {
    MetricsHash metrics;
    EXPECT_EQ(0U, metrics.size());
    metrics["a"] = 1;
    EXPECT_EQ(1U, metrics.size());
    metrics["b"] = 10;
    EXPECT_EQ(2U, metrics.size());
}

}
