//
// Created by grey on 27.05.16.
//

#ifndef QUAKEWITHSOCKETS_SESSION_H
#define QUAKEWITHSOCKETS_SESSION_H

#include <boost/enable_shared_from_this.hpp>
#include "message.h"

using boost::asio::ip::tcp;

typedef std::deque<message> message_queue;

class room {
public:
public:
    void join(participant_ptr participant)
    {
        participants_.insert(participant);
        for (auto msg: recent_msgs_)
            participant->deliver(msg);
    }

    void leave(participant_ptr participant)
    {
        participants_.erase(participant);
    }

    void deliver(const message& msg)
    {
        recent_msgs_.push_back(msg);
        while (recent_msgs_.size() > max_recent_msgs)
            recent_msgs_.pop_front();

        for (auto participant: participants_)
            participant->deliver(msg);
    }

private:
    std::set<participant_ptr> participants_;
    enum { max_recent_msgs = 100 };
    message_queue recent_msgs_;
};

class session : public participant, public std::enable_shared_from_this<session>{
public:
    session(tcp::socket socket, room& room) : socket_(std::move(socket)), room_(room) { }

    void start()
    {
        room_.join(shared_from_this());
        do_read_header();
    }

    void deliver(const message& msg)
    {
        bool write_in_progress = !write_msgs_.empty();
        write_msgs_.push_back(msg);
        if (!write_in_progress)
        {
            do_write();
        }
    }

private:
    void do_read_header()
    {
        auto self(shared_from_this());
        boost::asio::async_read(socket_,
                                boost::asio::buffer(read_msg_.data(), message::header_length),
                                [this, self](boost::system::error_code ec, std::size_t /*length*/)
                                {
                                    if (!ec && read_msg_.decode_header())
                                    {
                                        do_read_body();
                                    }
                                    else
                                    {
                                        room_.leave(shared_from_this());
                                    }
                                });
    }

    void do_read_body()
    {
        auto self(shared_from_this());
        boost::asio::async_read(socket_,
                                boost::asio::buffer(read_msg_.body(), read_msg_.body_length()),
                                [this, self](boost::system::error_code ec, std::size_t /*length*/)
                                {
                                    if (!ec)
                                    {
                                        room_.deliver(read_msg_);
                                        do_read_header();
                                    }
                                    else
                                    {
                                        room_.leave(shared_from_this());
                                    }
                                });
    }

    void do_write()
    {
        auto self(shared_from_this());
        boost::asio::async_write(socket_,
                                 boost::asio::buffer(write_msgs_.front().data(),
                                                     write_msgs_.front().length()),
                                 [this, self](boost::system::error_code ec, std::size_t /*length*/)
                                 {
                                     if (!ec)
                                     {
                                         write_msgs_.pop_front();
                                         if (!write_msgs_.empty())
                                         {
                                             do_write();
                                         }
                                     }
                                     else
                                     {
                                         room_.leave(shared_from_this());
                                     }
                                 });
    }

    tcp::socket socket_;
    room& room_;
    message read_msg_;
    message_queue write_msgs_;
};



#endif //QUAKEWITHSOCKETS_SESSION_H
