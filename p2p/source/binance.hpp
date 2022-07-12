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


#ifndef ORCHID_BINANCE_HPP
#define ORCHID_BINANCE_HPP

#include "exchange.hpp"

namespace orc {

class BinanceExchange :
    public Exchange
{
  private:
    const std::string key_;
    const Beam secret_;

  public:
    BinanceExchange(S<Base> base, std::string key, Beam secret) :
        Exchange(std::move(base)),
        key_(std::move(key)),
        secret_(std::move(secret))
    {
    }

    task<Any> Call(const std::string &method, const std::string &path, std::map<std::string, std::string> args) const {
        args["recvWindow"] = "1000";
        args["timestamp"] = std::to_string(Monotonic() / 1000);
        const auto query(Query(args));
        co_return Parse((co_await base_->Fetch(method, {{"https", "api.binance.us", "443"},
            F() << path << query << "&signature=" << Auth<Hash2, 64>(secret_, query.substr(1)).hex(false)
        }, {
            {"X-MBX-APIKEY", key_},
        }, {})).ok());
    }

    task<Portfolio> GetPortfolio() override {
        Portfolio portfolio;
        const auto account((co_await Call("GET", "/api/v3/account", {})).as_object());
        for (const auto &balance : account.at("balances").as_array())
            if (const auto amount = To<double>(Str(balance.at("free"))) + To<double>(Str(balance.at("locked"))))
                portfolio[Str(balance.at("asset"))] = amount;
        co_return portfolio;
    }
};

}

#endif//ORCHID_BINANCE_HPP
