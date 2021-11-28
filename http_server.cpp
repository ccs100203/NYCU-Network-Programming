#include <cstdlib>
#include <iostream>
#include <memory>
#include <utility>
#include <boost/asio.hpp>
#include <boost/algorithm/string.hpp>
#include <map>

using boost::asio::ip::tcp;
using namespace std;

boost::asio::io_context io_context;

class session
    : public enable_shared_from_this<session>
{
public:
    session(tcp::socket socket)
    : socket_(move(socket))
    {
    }

    void start()
    {
        do_read();
    }

private:
    void do_read()
    {
        auto self(shared_from_this());
        socket_.async_read_some(boost::asio::buffer(data_, max_length),
            [this, self](boost::system::error_code ec, size_t length)
            {
                if (!ec)
                {
                    /* response 200 OK */
                    do_write(length);

                    /* parse response data */
                    string str = data_;
                    boost::replace_all(str, "\r", "");
                    vector<string> strs;
                    boost::split(strs, str, boost::is_any_of(" \n"));

                    /* set environment variable */
                    std::map<string, string> envmap;
                    envmap["REQUEST_METHOD"] = strs.at(0);
                    envmap["REQUEST_URI"] = strs.at(1);
                    envmap["SERVER_PROTOCOL"] = strs.at(2);
                    envmap["HTTP_HOST"] = strs.at(4);
                    envmap["SERVER_ADDR"] = socket_.local_endpoint().address().to_string();
                    envmap["SERVER_PORT"] = to_string(socket_.local_endpoint().port());
                    envmap["REMOTE_ADDR"] = socket_.remote_endpoint().address().to_string();
                    envmap["REMOTE_PORT"] = to_string(socket_.remote_endpoint().port());

                    /* the execution cgi */
                    string cgi_str = "";
                    /* if find ?, then has query string */
                    if (strs.at(1).find("?") != std::string::npos) {
                        vector<string> query_str;
                        boost::split(query_str, strs.at(1), boost::is_any_of("?"));
                        envmap["QUERY_STRING"] = query_str.at(1);
                        cgi_str = query_str.at(0);
                    } else {
                        envmap["QUERY_STRING"] = "";
                        cgi_str = strs.at(1);
                    }
                        boost::replace_all(cgi_str, "/", "./");

                    // for (auto it: envmap) {
                    //     cout << it.first << " " << it.second << endl;
                    // }
                    cout << cgi_str << endl;

                    /* fork */
                    io_context.notify_fork(boost::asio::io_context::fork_prepare);
                    if (fork() == 0)
                    {
                        // This is the child process.
                        io_context.notify_fork(boost::asio::io_context::fork_child);
                        for (auto it: envmap) {
                            setenv(it.first.c_str(), it.second.c_str(), 1);
                        }

                        dup2(socket_.native_handle(), 0);
                        dup2(socket_.native_handle(), 1);
                        // dup2(socket_.native_handle(), 2);
                        socket_.close();

                        char *exec_argv[2] = {strdup(cgi_str.c_str()), NULL};
                        if (execvp(exec_argv[0], exec_argv) == -1) {
                            cerr << "execvp error\n";
                            exit(1);
                        }
                    }
                    else
                    {
                        // This is the parent process.
                        io_context.notify_fork(boost::asio::io_context::fork_parent);

                        socket_.close();
                    }
                    
                }
            });
    }

    void do_write(size_t length)
    {
        auto self(shared_from_this());
        char msg[30] = "HTTP/1.1 200 OK\r\n";
        boost::asio::async_write(socket_, boost::asio::buffer(msg, strlen(msg)),
            [this, self](boost::system::error_code ec, size_t /*length*/){});
    }

    tcp::socket socket_;
    enum { max_length = 4096 };
    char data_[max_length];

};

class server
{
public:
    server(short port)
    : acceptor_(io_context, tcp::endpoint(tcp::v4(), port))
    {
        // acceptor_.set_option(boost::asio::socket_base::reuse_address(true));
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
                make_shared<session>(move(socket))->start();
            }

            do_accept();
        });
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

        // boost::asio::io_context io_context;

        server s(atoi(argv[1]));

        io_context.run();
    }
    catch (exception& e)
    {
        cerr << "Exception: " << e.what() << "\n";
    }

  return 0;
}