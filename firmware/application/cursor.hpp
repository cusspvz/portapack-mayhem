/*
 * Copyright (C) 2022 José Moreira @cusspvz
 *
 * This file is part of PortaPack.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2, or (at your option)
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; see the file COPYING.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street,
 * Boston, MA 02110-1301, USA.
 */

struct cursor
{
public:
    uint32_t index{0};
    uint32_t total{0};

    void reset()
    {
        index = 0;
        total = 0;
    }

    void start_over()
    {
        index = 0;
    }

    void bump()
    {
        index++;
    }

    bool is_last()
    {
        return index == (total - 1) || index >= total;
    }

    bool is_done()
    {
        return index >= total;
    }
};
