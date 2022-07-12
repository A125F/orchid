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


#ifndef ORCHID_KRAKEN_HPP
#define ORCHID_KRAKEN_HPP

#include "exchange.hpp"

namespace orc {

class KrakenExchange :
    public Exchange
{
  private:
    const std::string key_;
    const Beam secret_;

    Any Call(Response response) const;

  public:
    KrakenExchange(S<Base> base, std::string key, Beam secret) :
        Exchange(std::move(base)),
        key_(std::move(key)),
        secret_(std::move(secret))
    {
    }

    task<Any> Get(const std::string &path, Parameters args) const;
    task<Any> Post(const std::string &path, Parameters args) const;

    task<Portfolio> GetPortfolio() override;
};

class KrakenBook :
    public Book
{
  public:
    KrakenBook(const Object &object);
};

}

#endif//ORCHID_KRAKEN_HPP
