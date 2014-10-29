/**
* Copyright (C) Mellanox Technologies Ltd. 2001-2013.  ALL RIGHTS RESERVED.
*
* $COPYRIGHT$
* $HEADER$
*/

#include <ucs/gtest/test.h>
extern "C" {
#include <ucs/stats/stats.h>
}

#include <boost/lexical_cast.hpp>
#include <sys/socket.h>
#include <netinet/in.h>

#if ENABLE_STATS

class stats_test : public ucs::test {
public:

    template <unsigned N>
    struct stats_class {
        ucs_stats_class_t cls;
        const char        *counter_names[N];
    };

    virtual void init() {
        ucs::test::init();
        ucs_stats_cleanup();
        push_config();
        set_config("STATS_DEST",    stats_dest_config().c_str());
        set_config("STATS_TRIGGER", stats_trigger_config().c_str());
        ucs_stats_init();
        ASSERT_TRUE(ucs_stats_is_active());
    }

    virtual void cleanup() {
        ucs_stats_cleanup();
        pop_config();
        ucs_stats_init();
        ucs::test::cleanup();
    }

    virtual std::string stats_dest_config()    = 0;
    virtual std::string stats_trigger_config() = 0;

    void prepare_nodes() {
        static stats_class<0> category_stats_class = {
            {"category", 0, {}}
        };

        static stats_class<4> data_stats_class = {
            { "data", NUM_COUNTERS, {} },
            { "counter0","counter1","counter2","counter3" }
        };

        ucs_status_t status = UCS_STATS_NODE_ALLOC(&cat_node, &category_stats_class.cls, NULL);
        ASSERT_UCS_OK(status);
        for (unsigned i = 0; i < NUM_DATA_NODES; ++i) {
            status = UCS_STATS_NODE_ALLOC(&data_nodes[i], &data_stats_class.cls,
                                         cat_node, "-%d", i);
            ASSERT_UCS_OK(status);

            UCS_STATS_UPDATE_COUNTER(data_nodes[i], 0, 10);
            UCS_STATS_UPDATE_COUNTER(data_nodes[i], 1, 20);
            UCS_STATS_UPDATE_COUNTER(data_nodes[i], 2, 30);
            UCS_STATS_UPDATE_COUNTER(data_nodes[i], 3, 40);
        }

        /* make sure our original node is ok */
        check_cat_node(cat_node);
    }

    void free_nodes() {
        for (unsigned i = 0; i < NUM_DATA_NODES; ++i) {
            UCS_STATS_NODE_FREE(data_nodes[i]);
        }
        UCS_STATS_NODE_FREE(cat_node);
    }

    void check_tree(ucs_stats_node_t *root) {
        EXPECT_EQ(1ul, ucs_list_length(&root->children[UCS_STATS_ACTIVE_CHILDREN]));
        check_cat_node(ucs_list_head(&root->children[UCS_STATS_ACTIVE_CHILDREN],
                                     ucs_stats_node_t, list));
    }

    void check_cat_node(ucs_stats_node_t *cat_node) {
        EXPECT_EQ(std::string("category"), std::string(cat_node->cls->name));
        EXPECT_EQ(0, cat_node->cls->num_counters);

        ucs_stats_node_t *data_node;
        ucs_list_for_each(data_node, &cat_node->children[UCS_STATS_ACTIVE_CHILDREN], list) {
            EXPECT_EQ(std::string("data"),     std::string(data_node->cls->name));
            EXPECT_EQ(unsigned(NUM_COUNTERS),  data_node->cls->num_counters);
            EXPECT_EQ(std::string("counter0"), std::string(data_node->cls->counter_names[0]));

            EXPECT_EQ(10, data_node->counters[0]);
            EXPECT_EQ(20, data_node->counters[1]);
            EXPECT_EQ(30, data_node->counters[2]);
            EXPECT_EQ(40, data_node->counters[3]);
        }
    }

protected:    
    static const unsigned NUM_DATA_NODES = 20;
    static const unsigned NUM_COUNTERS   = 4;

    ucs_stats_node_t       *cat_node;
    ucs_stats_node_t       *data_nodes[NUM_DATA_NODES];
};

class stats_udp_test : public stats_test {
public:
    virtual void init() {
        ucs_status_t status = ucs_stats_server_start(0, &m_server);
        ASSERT_UCS_OK(status);
        stats_test::init();
    }

    virtual void cleanup() {
        stats_test::cleanup();
        ucs_stats_server_destroy(m_server);
    }

    virtual std::string stats_dest_config() {
        int port = ucs_stats_server_get_port(m_server);
        EXPECT_GT(port, 0);
        return "udp:localhost:" + boost::lexical_cast<std::string>(port);
    }

    virtual std::string stats_trigger_config() {
        return "timer:0.1s";
    }

    void read_and_check_stats() {
        ucs_list_link_t *list = ucs_stats_server_get_stats(m_server);
        ASSERT_EQ(1ul, ucs_list_length(list));
        check_tree(ucs_list_head(list, ucs_stats_node_t, list));
        ucs_stats_server_purge_stats(m_server);
    }

protected:
    ucs_stats_server_h m_server;
};

class stats_file_test : public stats_test {
public:
    stats_file_test() {
        m_pipefds[0] = -1;
        m_pipefds[1] = -1;
    }

    virtual void init() {
        /* Note: this test assumes data <64k, o/w stats dump will block forever */
        int ret = pipe(m_pipefds);
        ASSERT_EQ(0, ret);
        stats_test::init();
    }

    void close_pipes()
    {
        close(m_pipefds[0]);
        close(m_pipefds[1]);
        m_pipefds[0] = -1;
        m_pipefds[1] = -1;
    }

    virtual void cleanup() {
        stats_test::cleanup();
        close_pipes();
    }

    virtual std::string stats_dest_config() {
        return "file:/dev/fd/" + boost::lexical_cast<std::string>(m_pipefds[1]) + ":bin";
    }

    std::string get_data() {
        std::string data(65536, '\0');
        ssize_t ret = read(m_pipefds[0], &data[0], data.size());
        EXPECT_GE(ret, 0);
        data.resize(ret);
        return data;
    }

    virtual std::string stats_trigger_config() {
        return "";
    }

protected:
    int m_pipefds[2];
};

class stats_on_demand_test : public stats_udp_test {
public:
    virtual std::string stats_trigger_config() {
        return "";
    }
};

class stats_on_signal_test : public stats_udp_test {
public:
    virtual std::string stats_trigger_config() {
        return "signal:USR1";
    }
};

class stats_on_exit_test : public stats_file_test {
public:
    virtual std::string stats_dest_config() {
        return "file:/dev/fd/" + boost::lexical_cast<std::string>(m_pipefds[1]);
    }

    /*
     * we check the dump-on-exit in cleanup method .
     */
    virtual void cleanup() {
        stats_test::cleanup();
        std::string data = get_data();
        size_t pos = 0;
        for (unsigned i = 0; i < NUM_DATA_NODES; ++i) {
            std::string node_name = " data-" + boost::lexical_cast<std::string>(i) + ":";
            pos = data.find(node_name, pos);
            EXPECT_NE(pos, std::string::npos) << node_name << " not found";
            for (unsigned j = 0; j < NUM_COUNTERS; ++j) {
                std::string value = "counter" +
                                boost::lexical_cast<std::string>(j) +
                                ": " +
                                boost::lexical_cast<std::string>((j + 1) * 10);
                pos = data.find(value, pos);
                EXPECT_NE(pos, std::string::npos) << value << " not found";
            }
        }
        close_pipes();
    }

    virtual std::string stats_trigger_config() {
        return "exit";
    }
};


UCS_TEST_F(stats_udp_test, report) {
    prepare_nodes();
    usleep(500 * 1000);
    read_and_check_stats();
    free_nodes();
}

UCS_TEST_F(stats_file_test, report) {
    prepare_nodes();
    ucs_stats_dump();
    free_nodes();

    std::string data = get_data();
    FILE *f = fmemopen(&data[0], data.size(), "rb");
    ucs_stats_node_t *root;
    ucs_status_t status = ucs_stats_deserialize(f, &root);
    ASSERT_UCS_OK(status);
    fclose(f);

    check_tree(root);
    ucs_stats_free(root);
}

UCS_TEST_F(stats_on_demand_test, report) {
    prepare_nodes();
    ucs_stats_dump();
    usleep(10000 * ucs::test_time_multiplier());
    read_and_check_stats();
    free_nodes();
}

UCS_TEST_F(stats_on_signal_test, report) {
    prepare_nodes();
    kill(getpid(), SIGUSR1);
    usleep(10000 * ucs::test_time_multiplier());
    read_and_check_stats();
    free_nodes();
}

UCS_TEST_F(stats_on_exit_test, dump) {
    prepare_nodes();
    free_nodes();
}

#endif