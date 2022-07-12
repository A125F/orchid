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


#include "kraken.hpp"

namespace orc {

Any KrakenExchange::Call(Response response) const {
    const auto object(Parse(std::move(response).ok()));
    const auto errors(object.at("error").as_array());
    orc_assert_(errors.empty(), Unparse(errors));
    return std::move(object.at("result"));
}

task<Any> KrakenExchange::Get(const std::string &path, Parameters args) const { orc_block({
    co_return Call(co_await base_->Fetch("POST", {{"https", "api.kraken.com", "443"}, path + Query(args)}, {}, {}));
}, "getting " << path + Query(args)); }

task<Any> KrakenExchange::Post(const std::string &path, Parameters args) const { orc_block({
    const auto nonce(std::to_string(Monotonic()));
    args["nonce"] = nonce;
    const auto body(Query(args).substr(1));

    co_return Call(co_await base_->Fetch("POST", {{"https", "api.kraken.com", "443"}, path}, {
        {"API-Key", key_},
        {"API-Sign", ToBase64(Auth<Hash4, 128>(secret_, Tie(path, Hash2(Tie(nonce, body)))))},
        {"Content-Type", "application/x-www-form-urlencoded; charset=utf-8"},
    }, body));
}, "posting " << path + Query(args)); }

task<Portfolio> KrakenExchange::GetPortfolio() {
    Portfolio portfolio;
    const auto balances((co_await Post("/0/private/Balance", {})).as_object());
    for (const auto &balance : balances)
        if (const auto amount = To<double>(Str(balance.value())))
            portfolio[std::string(balance.key())] = amount;
    co_return portfolio;
}

KrakenBook::KrakenBook(const Object &object) {
    for (const auto &i : object.at("bids").as_array()) {
        const auto &bid(i.as_array());
        orc_assert(bid.size() == 3);
        orders_.try_emplace(To<double>(Str(bid.at(0))), 1. * To<double>(Str(bid.at(1))));
    }
    for (const auto &i : object.at("asks").as_array()) {
        const auto &ask(i.as_array());
        orc_assert(ask.size() == 3);
        orders_.try_emplace(To<double>(Str(ask.at(0))), -1. * To<double>(Str(ask.at(1))));
    }
}

}
