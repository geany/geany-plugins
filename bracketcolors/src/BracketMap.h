/*
 *      BracketMap.h
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

#ifndef __BRACKET_MAP_H__
#define __BRACKET_MAP_H__

#include <map>
#include <tuple>
#include <set>

#include <glib.h>


// -----------------------------------------------------------------------------
    struct BracketMap
/*
    Purpose:    data structure which stores and computes nesting order
----------------------------------------------------------------------------- */
{
    typedef gint Length, Order, Index;
    typedef std::tuple<Length, Order> Bracket;
    std::map<Index, Bracket> mBracketMap;

    BracketMap();
    ~BracketMap();

    void Update(Index index, Length length);
    std::set<Index> ComputeOrder();

    static const gint UNDEFINED = -1;

    static Length& GetLength(Bracket &bracket) {
        return std::get<0>(bracket);
    }
    static Order& GetOrder(Bracket &bracket) {
        return std::get<1>(bracket);
    }

    static const Length& GetLength(const Bracket &bracket) {
        return std::get<0>(bracket);
    }
    static const Order& GetOrder(const Bracket &bracket) {
        return std::get<1>(bracket);
    }
};

#endif
