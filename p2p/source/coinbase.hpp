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


#ifndef ORCHID_COINBASE_HPP
#define ORCHID_COINBASE_HPP

#include "exchange.hpp"

namespace orc {

class CoinbaseExchange :
    public Exchange
{
  private:
    const std::string key_;
    const std::string passphrase_;
    const Beam secret_;

  public:
    CoinbaseExchange(S<Base> base, std::string key, std::string passphrase, Beam secret) :
        Exchange(std::move(base)),
        key_(std::move(key)),
        passphrase_(std::move(passphrase)),
        secret_(std::move(secret))
    {
    }

    task<Response> Call(const std::string &method, const std::string &path, const std::string &body) const;

    task<Object> call(const std::string &method, const std::string &path, const std::string &body = {}) const;
    task<Any> kill(const std::string &path, const std::string &body = {}) const;
    cppcoro::async_generator<Object> list(std::string method, std::map<std::string, std::string> args) const;

    task<Portfolio> GetPortfolio() override;
};

class CoinbaseBook :
    public Book
{
  public:
    void Set(const Object &event);
    task<void> Run(const S<Base> &base, const std::string &pair);
};

}

#endif//ORCHID_COINBASE_HPP
