#include <cstdlib>
#include <iostream>
#include <memory>
#include <utility>
#include <boost/asio.hpp>
#include <boost/algorithm/string.hpp>
#include <map>
#include <sys/wait.h>
#include <boost/array.hpp>
#include <regex>
#include <fstream>
#include <boost/regex.hpp>


using boost::asio::ip::tcp;
using namespace std;

boost::asio::io_context io_context;
boost::asio::signal_set signal_(io_context, SIGCHLD);

class session
    : public enable_shared_from_this<session>
{
public:
    session(tcp::socket socket)
    : socket_src(move(socket)),
      socket_dst(io_context)
    {
    }

    void start()
    {
        read_request();
    }

private:
    void read_request()
    {
        auto self(shared_from_this());
        memset(data_, 0, max_length);
        socket_src.async_read_some(boost::asio::buffer(data_, max_length),
            [this, self](boost::system::error_code ec, size_t length)
            {
                if (!ec) {
                    /* parse socks4 request */
                    parse_req();
                    check_firewall();
                    print_server_msg();
                    if (packet_map.at("REPLY") == "Reject") {
                        socks_reply("port");
                    } /* connect mode */
                    else if (packet_map.at("CD") == "1") {
                        socks_reply("port");
                        cout << "goto connect" << endl;
                        do_connect();
                    } /* bind mode */
                    else {

                    }
                }
            });
    }

    /* parse socks4 request */
    void parse_req()
    {
        /* version */
        packet_map["VN"] = to_string(data_[0]);
        /* command */
        packet_map["CD"] = to_string(data_[1]);
        packet_map["DST_PORT"] = to_string((data_[2] << 8 | (data_[3] & 0xff)) & 0xffff);

        string tmp = "";
        for (int i = 4; i < 7; ++i) {
            tmp += to_string(data_[i] & 0xff);
            tmp += ".";
        }
        tmp += to_string(data_[7] & 0xff);
        packet_map["DST_IP"] = tmp;

        packet_map["SRC_IP"] = socket_src.remote_endpoint().address().to_string();
        packet_map["SRC_PORT"] = to_string(socket_src.remote_endpoint().port());
    }

    void check_firewall()
    {
        std::ifstream file_stream("./socks.conf");
        string rule = "";
        packet_map["REPLY"] = "Reject";
        while (getline(file_stream, rule)) {
            boost::replace_all(rule, "*", "\\d+");
            boost::replace_all(rule, ".", "\\.");
            string exp = (packet_map.at("CD") == "1") ? "permit c " : "permit b ";
            exp += packet_map.at("DST_IP");
            if(rule[rule.length()-1] == '\r') {
                rule[rule.length()-1] = '\0';
            }

            if(std::regex_match(exp, std::regex(rule))) {
                packet_map["REPLY"] = "Accept";
                break;
            }
            rule = "";
        }
        file_stream.close();
    }

    void print_server_msg()
    {
        cout << "<S_IP>: " << packet_map.at("SRC_IP") << endl;
        cout << "<S_PORT>: " << packet_map.at("SRC_PORT") << endl;
        cout << "<D_IP>: " << packet_map.at("DST_IP") << endl;
        cout << "<D_PORT>: " << packet_map.at("DST_PORT") << endl;
        if (packet_map.at("CD") == "1")
            cout << "<Command>: " << "CONNECT" << endl;
        else
            cout << "<Command>: " << "BIND" << endl;
        cout << "<Reply>: " << packet_map.at("REPLY") << endl << endl;
    }

    // TODO
    void socks_reply(string port) {
        auto self(shared_from_this());
        string reply_msg = "";
        reply_msg += '\0';
        reply_msg += (packet_map.at("REPLY") == "Accept") ? 90 : 91;
        /* Connect */
        if (packet_map.at("CD") == "1") {
            reply_msg += "\0\0\0\0\0\0";
        } /* bind */
        else {

        }
        boost::asio::async_write(socket_src, boost::asio::buffer(reply_msg, reply_msg.length()),
        [this, self](boost::system::error_code ec, std::size_t /*length*/)
        {
            if (!ec)
            {
                
            } else {
                cerr << "socks_reply: " << ec.message() << endl;
            }
        });
    }

    void do_connect()
    {
        auto self(shared_from_this());
        tcp::resolver r_dst(io_context);
        tcp::resolver::results_type dst_endpoint = r_dst.resolve(packet_map["DST_IP"], packet_map["DST_PORT"]);
        tcp::resolver::results_type::iterator dst_it = dst_endpoint.begin();

        /* connect to destination */
        socket_dst.async_connect(dst_it->endpoint(), [this, self](boost::system::error_code ec)
        {
            if (!ec)
            {
                do_read_from_dst();
                do_read_from_src();
            } else {
                cerr << "do_connect: " << ec.message();
            }
        });
    }

    void do_read_from_src()
    {
        auto self(shared_from_this());
        memset(data_, 0, max_length);
        socket_src.async_read_some(boost::asio::buffer(data_, max_length),
            [this, self](boost::system::error_code ec, size_t length)
            {
                if (!ec)
                {
                    string tmp = data_;
                    memset(data_, 0, max_length);
                    do_write_to_dst(tmp);
                    
                } /* EOF */
                else if (ec.value() == 2) {
                    cerr << "do_read_from_src: " << ec.message() << endl;
                } else {
                    cerr << "do_read_from_src: " << ec.message() << endl;
                }
            });
    }

    void do_read_from_dst()
    {
        auto self(shared_from_this());
        memset(data_, 0, max_length);
        socket_dst.async_read_some(boost::asio::buffer(data_, max_length),
            [this, self](boost::system::error_code ec, size_t length)
            {
                if (!ec)
                {
                    string tmp = data_;
                    memset(data_, 0, max_length);
                    do_write_to_src(tmp);
                    
                } /* EOF */
                else if (ec.value() == 2) {
                    cerr << "do_read_from_dst: "  << ec.message() << endl;
                    socket_dst.close();
                    socket_src.close();
                    exit(EXIT_SUCCESS);
                } else {
                    cerr << "do_read_from_dst: "  << ec.message() << endl;
                }
            });
    }

    void do_write_to_src(string msg)
    {
        auto self(shared_from_this());
        boost::asio::async_write(socket_src, boost::asio::buffer(msg, msg.length()),
        [this, self](boost::system::error_code ec, std::size_t /*length*/)
        {
            if (!ec)
            {
                do_read_from_dst();
            } else {
                cerr << "do_write_to_src: "  << ec.message() << endl;
            }
        });
    }

    void do_write_to_dst(string msg)
    {
        auto self(shared_from_this());
        boost::asio::async_write(socket_dst, boost::asio::buffer(msg, msg.length()),
        [this, self](boost::system::error_code ec, std::size_t /*length*/)
        {
            if (!ec)
            {
                do_read_from_src();
            } else {
                cerr << "do_write_to_dst: "  << ec.message() << endl;
            }
        });
    }

    tcp::socket socket_src;
    tcp::socket socket_dst;
    enum { max_length = 4096 };
    char data_[max_length];
    std::map<string, string> packet_map;
};

class server
{
public:
    server(short port)
    : acceptor_(io_context, tcp::endpoint(tcp::v4(), port))
    {
        do_accept();
    }

private:
    void do_accept()
    {
    acceptor_.async_accept(
        [this](boost::system::error_code ec, tcp::socket socket)
        {
            if (!ec)
            {
                /* fork */
                io_context.notify_fork(boost::asio::io_context::fork_prepare);
                if (fork() == 0)
                {
                    /* This is the child process. */
                    io_context.notify_fork(boost::asio::io_context::fork_child);
                    acceptor_.close();
                    // cout << "It's child" << endl;
                    make_shared<session>(move(socket))->start();
                }
                else
                {
                    /* This is the parent process. */
                    io_context.notify_fork(boost::asio::io_context::fork_parent);
                    // cout << "It's parent" << endl;
                    signal_.async_wait(handler);
                    socket.close();
                }
            }

            do_accept();
        });
    }

    /* signal handler */
    static void handler(const boost::system::error_code& error, int signal_number)
    {
        if (!error)
        {
            /* A signal occurred. */
            while (waitpid(-1, NULL, WNOHANG) > 0);
        }
    }

    tcp::acceptor acceptor_;
};

int main(int argc, char* argv[])
{
    try
    {
        if (argc != 2)
        {
            cerr << "Usage: async_tcp_echo_server <port>\n";
            return 1;
        }

        server s(atoi(argv[1]));

        io_context.run();
    }
    catch (exception& e)
    {
        cerr << "Exception: " << e.what() << "\n";
    }

  return 0;
}