#pragma once
#include "handlers.h"
#include "services/session_registry.h"
#include <boost/asio/io_context.hpp>

struct SessionFixture {
    boost::asio::io_context io;
    tmapsvr::InMemorySessionRegistry registry;
    std::shared_ptr<tnetlib::AsioSession> session;
    SessionFixture(tmapsvr::HandlerContext& ctx, std::uint32_t cid, std::uint32_t key,
        std::uint8_t channel = 1) {
        boost::asio::ip::tcp::socket socket(io);
        socket.open(boost::asio::ip::tcp::v4());
        session = std::make_shared<tnetlib::AsioSession>(std::move(socket), tnetlib::PeerType::Server);
        registry.TryBind({cid, 17, key, channel}, session);
        ctx.session_reg = &registry;
    }
};
