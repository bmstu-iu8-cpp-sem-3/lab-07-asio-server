// Copyright 2021 Malenko Artem artem.malenko@yandex.ru

#include <header.hpp>

using boost::asio::io_service;
using boost::asio::ip::tcp;
static io_service service_;
boost::recursive_mutex mutex_;

class talk_to_client {
private:
    std::chrono::system_clock::
    time_point time;
    std::string username;
    boost::asio::ip::tcp::socket socket_;
public:
    talk_to_client() : time(std::chrono::system_clock::now()), socket_(service_) {}

    tcp::socket &get_socket() {
        return socket_;
    }

    std::string get_username() {
        return username;
    }

    void reset_username(std::string &new_username) {
        username = new_username;
    }

    void answer_to_the_client(const std::string &an_answer) {
        socket_.write_some(boost::asio::buffer(an_answer));
    }

    bool check_time_out() {
        return (std::chrono::system_clock::now() - time).count() >= 5;
    }


};

typedef boost::shared_ptr <talk_to_client> client_ptr;
typedef std::vector <client_ptr> client_vector;
client_vector clients;
boost::recursive_mutex cs;

std::string get_users() {
    std::string result;
    for (auto &client : clients) result += client->get_username() + " ";
    return result;
}

void accept_thread() {
    tcp::acceptor acceptor(service_, tcp::endpoint(tcp::v4(), 8001));
    while (true) {
        BOOST_LOG_TRIVIAL(info)
                << "Start logging \n Thread id: " << std::this_thread::get_id() << "\n";
        client_ptr new_(new talk_to_client());
        acceptor.accept(new_->get_socket());
        boost::recursive_mutex::scoped_lock lk(cs);

        std::string username_;
        new_->get_socket().read_some(boost::asio::buffer(username_, 20));
        if (username_.empty()) {
            new_->answer_to_the_client("Login failed");
            BOOST_LOG_TRIVIAL(info) << "Logging failed \n Thread id: \n"
                                    << std::this_thread::get_id() << "\n";
        } else {
            new_->reset_username(username_);
            clients.push_back(new_);
            new_->answer_to_the_client("Login OK");
            BOOST_LOG_TRIVIAL(info)
                    << "Logging ok \n username: " << username_ << "\n";
        }
    }
}

void handle_clients_thread() {
    while (true) {
        boost::this_thread::sleep(boost::posix_time::millisec(1));
        boost::recursive_mutex::scoped_lock lk(cs);
        for (auto &client : clients) {
            std::string command;
            client->get_socket().read_some(boost::asio::buffer(command, 1024));
            if (command == "client_list_chaned")
                client->answer_to_the_client(get_users());
            client->answer_to_the_client("Ping OK");
            BOOST_LOG_TRIVIAL(info)
                    << "Answered to client \n username: " << client->get_username() << "\n";
        }
        clients.erase(std::remove_if(clients.begin(), clients.end(), boost::bind(&talk_to_client::check_time_out, _1)),
                      clients.end());
    }
}

int main() {
    boost::thread_group threads;
    threads.create_thread(accept_thread);
    threads.create_thread(handle_clients_thread);
    threads.join_all();
}
