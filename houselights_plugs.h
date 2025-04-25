/* houseslights - A simple home web server for lighting control
 *
 * Copyright 2020, Pascal Martin
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA  02110-1301, USA.
 *
 *
 * houselights_plugs.h - Control the light plugs.
 *
 */
void houselights_plugs_on
         (const char *name, int pulse, int manual, const char *cause);
void houselights_plugs_off (const char *name, int manual, const char *cause);

void houselights_plugs_periodic (time_t now);

int houselights_plugs_status (char *buffer, int size);

