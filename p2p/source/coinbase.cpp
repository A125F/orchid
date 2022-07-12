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


#include "coinbase.hpp"
#include "duplex.hpp"

namespace orc {

task<Response> CoinbaseExchange::Call(const std::string &method, const std::string &path, const std::string &body) const {
    const auto timestamp(std::to_string(Timestamp()));
    const auto signature(ToBase64(Auth<Hash2, 64>(secret_, Tie(timestamp, method, path, body))));

    co_return co_await base_->Fetch(method, {{"https", "api.pro.coinbase.com", "443"}, path}, {
        {"CB-ACCESS-KEY", key_},
        {"CB-ACCESS-SIGN", signature},
        {"CB-ACCESS-TIMESTAMP", timestamp},
        {"CB-ACCESS-PASSPHRASE", passphrase_},
        {"Content-Type", "application/json"},
    }, body);
}

task<Object> CoinbaseExchange::call(const std::string &method, const std::string &path, const std::string &body) const {
    co_return Parse((co_await Call(method, path, body)).ok()).as_object();
}

task<Any> CoinbaseExchange::kill(const std::string &path, const std::string &body) const {
    co_return Parse((co_await Call("DELETE", path, body)).ok());
}

cppcoro::async_generator<Object> CoinbaseExchange::list(std::string method, std::map<std::string, std::string> args) const {
    for (;;) {
        const auto path([&]() {
            std::ostringstream path;
            path << '/' << method;

            bool ampersand(false);
            for (const auto &arg : args) {
                if (ampersand)
                    path << '&';
                else {
                    path << '?';
                    ampersand = true;
                }

                path << arg.first << '=' << arg.second;
            }

            return path.str();
        }());

        auto response(co_await Call("GET", path, {}));
        orc_assert_(response.result() == http::status::ok, response.body());

        auto body(Parse(response.body()));
        for (auto &value : body.as_array())
            co_yield std::move(value.as_object());

        const auto after(response.find("CB-AFTER"));
        if (after == response.end())
            break;
        args["after"] = Str(after->value());
    }
}

task<Portfolio> CoinbaseExchange::GetPortfolio() {
    Portfolio portfolio;
    for co_await (const auto &account : list("accounts", {}))
        if (const auto amount = To<double>(Str(account.at("balance"))))
            portfolio[Str(account.at("currency"))] = amount;
    co_return portfolio;
}

void CoinbaseBook::Set(const Object &event) {
    orders_.clear();
    for (const auto &i : event.at("bids").as_array()) {
        const auto &bid(i.as_array());
        orc_assert(bid.size() >= 2);
        orders_[To<double>(Str(bid.at(0)))] += 1. * To<double>(Str(bid.at(1)));
    }
    for (const auto &i : event.at("asks").as_array()) {
        const auto &ask(i.as_array());
        orc_assert(ask.size() >= 2);
        orders_[To<double>(Str(ask.at(0)))] += -1. * To<double>(Str(ask.at(1)));
    }
}

task<void> CoinbaseBook::Run(const S<Base> &base, const std::string &pair) {
    const auto feed(co_await Duplex(base, "wss://ws-feed.pro.coinbase.com/"));

    co_await feed->Send(Strung(Unparse({
        {"type", "subscribe"},
        {"channels", {
            {{"name", "level2"}, {"product_ids", {pair}}},
        }},
    })));

    for (Beam data(1024*1024*32);;) {
        const auto writ(co_await feed->Read(data));
        const auto event(Parse(data.subset(0, writ).str()).as_object());
        const auto type(Str(event.at("type")));
        //std::cout << event << std::endl;
        if (false) {

        } else if (type == "snapshot") {
            Set(event);
        } else if (type == "l2update") {
            for (const auto &i : event.at("changes").as_array()) {
                const auto &change(i.as_array());
                orc_assert(change.size() == 3);
                const auto side(Side(Str(change.at(0))));
                const auto price(To<double>(Str(change.at(1))));
                const auto size(To<double>(Str(change.at(2))));
                if (size == 0)
                    orders_.erase(price);
                else
                    orders_[price] = side * size;
            }
        } else {
            std::cout << event << std::endl;
        }
    }
}

}
