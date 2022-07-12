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


#include "exchange.hpp"

namespace orc {

double Book::Mid() const {
    double before;
    for (const auto &[price, amount] : orders_)
        if (amount < 0)
            return (price + before) / 2;
        else
            before = price;
    return 0;
}

double Book::Liquidity(double depth) const {
    const auto mid(Mid());
    const auto under(mid / depth);
    const auto over(mid * depth);

    double liquidity(0);
    for (const auto &[price, amount] : orders_)
        if (price >= under && price <= over)
            liquidity += price * std::abs(amount);

    return liquidity;
}

}
