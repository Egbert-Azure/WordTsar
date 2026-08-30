//////////////////////////////////////////////////////////////////////////////
//
// WordTsar - Wordstar clone for modern systems http://wordtsar.ca
// Copyright (C) 2018 Gerald Brandt
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU Affero General Public License as
// published by the Free Software Foundation, either version 3 of the
// License, or (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU Affero General Public License for more details.
//
// You should have received a copy of the GNU Affero General Public License
// along with this program.  If not, see <http://www.gnu.org/licenses/>.
//
//////////////////////////////////////////////////////////////////////////////

#ifndef TIMER_H
#define TIMER_H

#include <chrono>

using namespace std::chrono;

//typedef high_resolution_clock Clock;
typedef steady_clock Clock;

class cTimer
{
public:

    void start()
    {
        epoch = Clock::now();
    }

    long long time_elapsed_nanoseconds() const
    {
        Clock::duration t = std::chrono::duration_cast<std::chrono::nanoseconds>(Clock::now() - epoch) ;
        return t.count() ;
    }

    long long time_elapsed_milliseconds() const
    {
        Clock::duration t = std::chrono::duration_cast<std::chrono::milliseconds>(Clock::now() - epoch) ;
        return t.count() ;
    }

    long long time_elapsed_seconds() const
    {
        Clock::duration t = std::chrono::duration_cast<std::chrono::seconds>(Clock::now() - epoch) ;
        return t.count() ;
    }

private:
    Clock::time_point epoch;

};


#endif // TIMER_H
