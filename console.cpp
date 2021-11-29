#include <cstdlib>
#include <iostream>
#include <memory>
#include <utility>
#include <boost/asio.hpp>
#include <boost/algorithm/string.hpp>
#include <map>
#include <sys/wait.h>
#include <fstream>


using boost::asio::ip::tcp;
using namespace std;

boost::asio::io_context io_context;

/* map for recording GET query string */
std::map<string, string> querymap;
/* number of np_single_golden */
int host_number = 0;

class client : public enable_shared_from_this<client>
{
public:
    client(string session, string file, int id) : socket_(io_context), 
                session(session), data_(""), file_stream("./test_case/" + file),
                resolver_(io_context), hostId(id)
    {
    }

    void start()
    {
        auto self(shared_from_this());
        // Start the connect actor.
        tcp::resolver::query q(querymap.at("h"+to_string(hostId)), querymap.at("p"+to_string(hostId)));
        resolver_.async_resolve(q, [this, self](boost::system::error_code ec, tcp::resolver::iterator it)
        {
            if (!ec)
            {
                boost::asio::ip::tcp::endpoint endpoint = *it;
                socket_.async_connect(endpoint, [this, self](boost::system::error_code ec)
                {
                    if (!ec)
                    {
                        do_read();
                    } else {
                        cerr << "CONNECT ERROR" << endl;
                    }
                });
            }
        });

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
                    string tmp = data_;
                    output_shell(session, data_);
                    memset(data_, 0, max_length);
                    /* read complete */
                    if (tmp.find("%") != std::string::npos) {
                        do_write();
                    } else {
                        do_read();
                    }
                    
                } else {
                    cerr << "READ: " << ec.message() << endl;
                }
            });

    }

    void do_write()
    {
        auto self(shared_from_this());

        string command = "";
        getline(file_stream, command);
        command += "\n";

        cerr << command << flush;
        output_command(session, command);

        boost::asio::async_write(socket_, boost::asio::buffer(command, command.length()),
        [this, self](boost::system::error_code ec, std::size_t /*length*/)
        {
            if (!ec)
            {
                do_read();
            } else {
                cerr << "WRITE: " << ec.message() << endl;
            }
        });
    }

    void output_shell(string session, string content)
    {
        string str = content;
        boost::replace_all(str, "&", "&amp;");
        boost::replace_all(str, "<", "&lt;");
        boost::replace_all(str, ">", "&gt;");
        boost::replace_all(str, "\'", "&#39;");
        boost::replace_all(str, "\"", "&quot;");
        boost::replace_all(str, "\n", "&NewLine;");
        boost::replace_all(str, "\r", "");
        string output = "<script>document.getElementById('" + session + "').innerHTML += '" + str + "';</script>";
        cout << output << flush;
    }

    void output_command(string session, string content)
    {
        string str = content;
        boost::replace_all(str, "&", "&amp;");
        boost::replace_all(str, "<", "&lt;");
        boost::replace_all(str, ">", "&gt;");
        boost::replace_all(str, "\'", "&#39;");
        boost::replace_all(str, "\"", "&quot;");
        boost::replace_all(str, "\n", "&NewLine;");
        boost::replace_all(str, "\r", "");
        string output = "<script>document.getElementById('" + session + "').innerHTML += '<b>" + str + "</b>';</script>";
        cout << output << flush;
    }

    tcp::socket socket_;
    string session;
    enum { max_length = 2048 };
    char data_[max_length];
    std::ifstream file_stream;
    tcp::resolver resolver_;
    int hostId;
};

/* setting html header & format */
void output_init_html() {

    /* "<th scope=\"col\">nplinux1.cs.nctu.edu.tw:1234</th>\n" */
    /* "<td><pre id=\"s0\" class=\"mb-0\"></pre></td>\n" */
    string str1 = "", str2 = "";
    for (int i = 0; i < host_number; i++) {
        str1 += "<th scope=\"col\">";
        str1 += querymap.at("h"+to_string(i));
        str1 += ":";
        str1 += querymap.at("p"+to_string(i));
        str1 += "</th>\n";

        str2 += "<td><pre id=\"";
        str2 += "s"+to_string(i);
        str2 += "\" class=\"mb-0\"></pre></td>\n";
    }

    string html = 
    "<!DOCTYPE html>\n"
    "<html lang=\"en\">\n"
    "<head>\n"
        "<meta charset=\"UTF-8\" />\n"
        "<title>NP Project 3 My Console</title>\n"
        "<link\n"
        "rel=\"stylesheet\"\n"
        "href=\"https://cdn.jsdelivr.net/npm/bootstrap@4.5.3/dist/css/bootstrap.min.css\"\n"
        "integrity=\"sha384-TX8t27EcRE3e/ihU7zmQxVncDAy5uIKz4rEkgIXeMed4M0jlfIDPvg6uqKI2xXr2\"\n"
        "crossorigin=\"anonymous\"\n"
        "/>\n"
        "<link\n"
        "href=\"https://fonts.googleapis.com/css?family=Source+Code+Pro\"\n"
        "rel=\"stylesheet\"\n"
        "/>\n"
        "<link\n"
        "rel=\"icon\"\n"
        "type=\"image/png\"\n"
        "href=\"https://cdn0.iconfinder.com/data/icons/small-n-flat/24/678068-terminal-512.png\"\n"
        "/>\n"
        "<style>\n"
        "* {\n"
            "font-family: 'Source Code Pro', monospace;\n"
            "font-size: 1rem !important;\n"
        "}\n"
        "body {\n"
            "background-color: #212529;\n"
        "}\n"
        "pre {\n"
            "color: #cccccc;\n"
        "}\n"
        "b {\n"
            "color: #01b468;\n"
        "}\n"
        "</style>\n"
    "</head>\n"
    "<body>\n"
        "<table class=\"table table-dark table-bordered\">\n"
        "<thead>\n"
            "<tr>\n";
    html += str1;
    html += "</tr>\n"
        "</thead>\n"
        "<tbody>\n"
            "<tr>\n";
    html += str2;
    html += "</tr>\n"
        "</tbody>\n"
        "</table>\n"
    "</body>\n"
    "</html>\n";
    cout << html << flush;
}


/* parsing GET query string*/
void parse_query()
{
    /* get query string from environment*/
    string query_str = getenv("QUERY_STRING");
        
    vector<string> strs;
    boost::split(strs, query_str, boost::is_any_of("&"));

    /* split foreach parameter */
    for (auto it: strs) {
        vector<string> tmp_str;
        boost::split(tmp_str, it, boost::is_any_of("="));
        /* store key & value from a parameter into map */
        querymap[tmp_str.at(0)] = tmp_str.at(1);
        /* calculate how many hosts */
        if (tmp_str.at(1) != "") {
            host_number++;
        }
    }

    host_number /= 3;
}


int main(int argc, char* argv[]) 
{
    try
    {
        /* setting http header */
        cout << "Content-type: text/html\r\n\r\n";

        parse_query();
        output_init_html();
        
        for (int i = 0; i < host_number; i++) {
            make_shared<client>("s"+to_string(i), querymap.at("f"+to_string(i)), i)->start();
        }

        io_context.run();
    }
    catch (exception& e)
    {
        cerr << "Exception: " << e.what() << "\n";
    }
    return 0;
}