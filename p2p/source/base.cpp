/* Orchid - WebRTC P2P VPN Market (on Ethereum)
 * Copyright (C) 2017-2020  The Orchid Authors
*/

/* GNU Affero General Public License, Version 3 {{{ */
/*
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Affero General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.

 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Affero General Public License for more details.

 * You should have received a copy of the GNU Affero General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
**/
/* }}} */


#include <p2p/base/basic_packet_socket_factory.h>
#include <p2p/client/basic_port_allocator.h>
#include <rtc_base/network.h>

#include "base.hpp"
#include "locator.hpp"
#include "pirate.hpp"

namespace orc {

Base::Base(const char *type, U<rtc::NetworkManager> manager) :
    Valve(type),
    manager_(std::move(manager)),
    cache_(*this)
{
}

Base::~Base() = default;

struct Thread_ { typedef rtc::Thread *(rtc::BasicPacketSocketFactory::*type); };
template struct Pirate<Thread_, &rtc::BasicPacketSocketFactory::thread_>;

U<cricket::PortAllocator> Base::Allocator() {
    auto &factory(Factory());
    const auto thread(factory.*Loot<Thread_>::pointer);
    return thread->Invoke<U<cricket::PortAllocator>>(RTC_FROM_HERE, [&]() {
        return std::make_unique<cricket::BasicPortAllocator>(manager_.get(), &factory);
    });
}

// XXX: for Local::Fetch, this should use NSURLSession on __APPLE__

task<Response> Base::Fetch(const std::string &method, const Locator &locator, const std::map<std::string, std::string> &headers, const std::string &data, const std::function<bool (const std::list<const rtc::OpenSSLCertificate> &)> &verify) {
    co_return co_await orc::Fetch(*this, method, locator, headers, data, verify);
}

}
