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


#ifndef ORCHID_EXCHANGE_HPP
#define ORCHID_EXCHANGE_HPP

#include <cppcoro/async_generator.hpp>

#include <boost/date_time/posix_time/time_parsers.hpp>
#include <boost/math/special_functions/sign.hpp>

#include "base.hpp"
#include "base64.hpp"
#include "crypto.hpp"
#include "decimal.hpp"
#include "float.hpp"
#include "format.hpp"
#include "notation.hpp"
#include "request.hpp"
#include "time.hpp"

namespace orc {

struct Portfolio :
    std::map<std::string, double>
{
    Portfolio() = default;

    Portfolio(std::map<std::string, double> data) :
        std::map<std::string, double>(std::move(data))
    {
    }

    double get(const std::string &key) const {
        const auto value(find(key));
        if (value == end()) return {};
        return value->second;
    }
};

static inline std::ostream &operator <<(std::ostream &out, const Portfolio &portfolio) {
    out << "{";
    for (const auto &[name, amount] : portfolio)
        out << name << "=" << amount << ",";
    out << "}";
    return out;
}

template <typename Type_ = double>
inline Type_ Side(const std::string_view &value) {
    if (false);
    else if (value == "buy")
        return 1;
    else if (value == "sell")
        return -1;
    else orc_assert(false);
}

template <typename Type_>
Type_ Sign(const Type_ &value) {
    return boost::math::signbit(value) ? -1 : 1;
}

inline const char *Side(double value) {
    if (false);
    else if (value > 0)
        return "buy";
    else if (value < 0)
        return "sell";
    else orc_assert(false);
}

template <typename Type_>
struct Trade {
    boost::posix_time::ptime traded_;
    Type_ price_;
    Type_ volume_;
    Object extra_;
};

class Book {
  protected:
    std::map<double, double> orders_;

  public:
    double Mid() const;
    double Liquidity(double depth) const;
};

class Exchange {
  protected:
    const S<Base> base_;

  public:
    Exchange(S<Base> base) :
        base_(std::move(base))
    {
    }

    virtual task<Portfolio> GetPortfolio() = 0;
};

}

#endif//ORCHID_EXCHANGE_HPP
