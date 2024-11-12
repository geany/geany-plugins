/*
 *      BracketMap.cc
 *
 *      Copyright 2023 Asif Amin <asifamin@utexas.edu>
 *
 *      This program is free software; you can redistribute it and/or modify
 *      it under the terms of the GNU General Public License as published by
 *      the Free Software Foundation; either version 2 of the License, or
 *      (at your option) any later version.
 *
 *      This program is distributed in the hope that it will be useful,
 *      but WITHOUT ANY WARRANTY; without even the implied warranty of
 *      MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *      GNU General Public License for more details.
 *      You should have received a copy of the GNU General Public License
 *      along with this program; if not, write to the Free Software
 *      Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
 *      MA 02110-1301, USA.
 */

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <stack>
#include <set>

#include "BracketMap.h"


// -----------------------------------------------------------------------------
    BracketMap::BracketMap()
/*
    Constructor
----------------------------------------------------------------------------- */
{

}


// -----------------------------------------------------------------------------
    BracketMap::~BracketMap()
/*
    Destructor
----------------------------------------------------------------------------- */
{

}


// -----------------------------------------------------------------------------
    void BracketMap::Update(Index index, Length length)
/*

----------------------------------------------------------------------------- */
{
    auto it = mBracketMap.find(index);
    if (it != mBracketMap.end()) {
        auto &bracket = it->second;
        GetLength(bracket) = length;
    }
    else {
        mBracketMap.insert(
            std::make_pair(index, std::make_tuple(length, 0))
        );
    }
}


// -----------------------------------------------------------------------------
    std::set<BracketMap::Index> BracketMap::ComputeOrder()
/*

----------------------------------------------------------------------------- */
{
    std::stack<Index> orderStack;
    std::set<Index> updatedBrackets;

    for (auto &it : mBracketMap) {

        const Index &startIndex = it.first;
        Bracket &bracket = it.second;
        Length length = GetLength(bracket);
        Index endPos = startIndex + length;

        if (length == UNDEFINED) {
            // Invalid brackets
            GetOrder(bracket) = UNDEFINED;
            continue;
        }


        if (orderStack.size() == 0) {
            // First bracket
            orderStack.push(endPos);
        }
        else if (startIndex > orderStack.top()) {
            // not nested
            while(orderStack.size() > 0 and orderStack.top() < startIndex) {
                orderStack.pop();
            }
            orderStack.push(endPos);
        }
        else {
            // nested bracket
            orderStack.push(endPos);
        }

        Order newOrder = orderStack.size() - 1;
        Order currOrder = GetOrder(bracket);
        if (newOrder != currOrder) {
            updatedBrackets.insert(startIndex);
        }

        GetOrder(bracket) = newOrder;
    }

    return updatedBrackets;
}
