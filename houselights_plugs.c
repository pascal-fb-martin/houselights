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
 * houselights_plugs.c - Control the light plugs.
 *
 * SYNOPSYS:
 *
 * This module handles lighting plugs, including:
 * - Run periodic discoveries to find which server controls each plug.
 * - Turn each plug on or off as requested. The requestor may be the
 *   schedule function, or a manual request from the outside.
 *
 * This module is not configured by the user: it learns about a plug when
 * the other modules want to control it. Its job, really, is to find what
 * web service controls that plug.
 *
 * A plug that is not known to any active web service is eventually removed.
 *
 * void houselights_plugs_on  (const char *name, int pulse);
 * void houselights_plugs_off (const char *name);
 *
 *    Control one plug on of off. Note that, since these are lights, we do
 *    not apply a pulse on the 'off' state. The pulse is meant to protect
 *    against leaving a light on and wasting electricity.
 *
 * void houselights_plugs_periodic (time_t now);
 *
 *    The periodic function that runs the lights discovery logic.
 *
 * int houselights_plugs_status (char *buffer, int size);
 *
 *    A function that populates a complete status in JSON.
 *
 */

#include <string.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>

#include <echttp.h>
#include <echttp_json.h>
#include <echttp_encoding.h>

#include "houselog.h"
#include "housediscover.h"

#include "houselights_plugs.h"

#define DEBUG if (echttp_isdebug()) printf

#define DEFAULTSERVER "http://localhost/relay"

static char **Providers;
static int    ProvidersSize = 0;
static int    ProvidersCount = 0;

typedef struct {
    char *name;
    const char *commanded;
    char state[8];
    int countdown;
    int pulse;
    time_t requested;
    time_t deadline;
    char manual;
    char is_light;
    char status; // u: unmapped, i: idle, a: active (pending).
    char url[256];
} LightPlug;

#define MAX_LIFE  3

static LightPlug *Plugs = 0;
static int       PlugsSize = 0;
static int       PlugsCount = 0;

#define PLUG_ON_LIMIT (8*60*60)    // do not set a light on for longer
#define PLUG_CONTROL_EXPIRATION 60 // Do not retry for longer than this.

static void houselights_plugs_submit (int plug);

static int houselights_plugs_search (const char *name) {
    int i;
    int free = -1;

    for (i = PlugsCount-1; i >= 0; --i) {
        if (Plugs[i].name) {
            if (!strcmp (name, Plugs[i].name)) return i;
        } else {
            free = i;
        }
    }
    if (free < 0) {
        if (PlugsCount >= PlugsSize) {
            PlugsSize += 32;
            Plugs = realloc (Plugs, PlugsSize * sizeof(LightPlug));
        }
        free = PlugsCount++;
    }
    Plugs[free].name = strdup(name);
    Plugs[free].countdown = MAX_LIFE;
    Plugs[free].commanded = 0;
    Plugs[free].requested = 0;
    Plugs[free].deadline = 0;
    Plugs[free].state[0] = 0;
    Plugs[free].pulse = 0;
    Plugs[free].manual = 0;
    Plugs[free].is_light = 0;
    Plugs[free].status = 'u';
    Plugs[free].url[0] = 0;

    return free;
}

static const char *houselights_plugs_provider_keep (const char *provider) {
    int i;
    for (i = 0; i < ProvidersCount; ++i) {
        if (!strcmp (Providers[i], provider)) return Providers[i];
    }
    if (ProvidersCount >= ProvidersSize) {
        ProvidersSize += 16;
        Providers = realloc (Providers, ProvidersSize * sizeof(char *));
    }

    return Providers[ProvidersCount++] = strdup(provider); // Keep the string.
}

static int houselights_plugs_pending (int plug) {

    // Find all the cases when we would not need or want to issue a control.
    //
    time_t now = time(0);
    if (Plugs[plug].requested + PLUG_CONTROL_EXPIRATION < now) return 0;
    if ((Plugs[plug].deadline > 0) && (Plugs[plug].deadline <= now)) return 0;
    if (!Plugs[plug].commanded) return 0;
    if (!Plugs[plug].state) return 0;
    if (!strcmp (Plugs[plug].state, Plugs[plug].commanded)) return 0;
    if (!strcmp (Plugs[plug].state, "silent")) return 0;

    return 1; // No reason for not submitting a control.
}

static void houselights_plugs_discovery (const char *provider,
                                         char *data, int length) {

   ParserToken tokens[100];
   int  innerlist[100];
   char path[256];
   int  count = 100;
   int  i;

   time_t now = time(0);

   // Analyze the answer and retrieve the control points matching our plugs.
   const char *error = echttp_json_parse (data, tokens, &count);
   if (error) {
       houselog_trace
           (HOUSE_FAILURE, provider, "JSON syntax error, %s", error);
       return;
   }
   if (count <= 0) {
       houselog_trace (HOUSE_FAILURE, provider, "no data");
       return;
   }

   int controls = echttp_json_search (tokens, ".control.status");
   if (controls <= 0) {
       houselog_trace (HOUSE_FAILURE, provider, "no plug data");
       return;
   }

   int n = tokens[controls].length;
   if (n <= 0) {
       houselog_trace (HOUSE_FAILURE, provider, "empty plug data");
       return;
   }

   error = echttp_json_enumerate (tokens+controls, innerlist);
   if (error) {
       houselog_trace (HOUSE_FAILURE, path, "%s", error);
       return;
   }

   for (i = 0; i < n; ++i) {
       ParserToken *inner = tokens + controls + innerlist[i];

       // Only consider lights. Still accept no gear field (compatibility)
       int gear = echttp_json_search (inner, ".gear");
       if (gear >= 0 && strcmp (inner[gear].value.string, "light")) continue;

       int plug = houselights_plugs_search (inner->key);
       if (plug < 0) continue;

       int state = echttp_json_search (inner, ".state");
       if (state >= 0) {
           if (strcmp (Plugs[plug].state, inner[state].value.string)) {
               int hasstate = (Plugs[plug].state[0] > 0);
               strncpy (Plugs[plug].state,
                        inner[state].value.string, sizeof(Plugs[0].state));
               if (hasstate) {
                   // Do not report the initial state acquisition as a change.
                   houselog_event ("PLUG", Plugs[plug].name, "CHANGED",
                                   "TO %s", Plugs[plug].state);
               }
           }
       }

       if (strcmp (Plugs[plug].url, provider)) {
           if (Plugs[plug].url[0]) {
               // A change of server is very unusual. Let store these events.
               houselog_event ("PLUG", Plugs[plug].name , "ROUTE",
                               "CHANGED FROM %s TO %s", Plugs[plug].url, provider);
           } else {
               houselog_event_local ("PLUG", Plugs[plug].name, "ROUTE",
                                     "SET TO %s", provider);
           }
           snprintf (Plugs[plug].url, sizeof(Plugs[plug].url), provider);
           if (Plugs[plug].status == 'u') Plugs[plug].status = 'i';

           DEBUG ("Plug %s discovered on %s\n",
                  Plugs[plug].name, Plugs[plug].url);

           // If we discovered a plug for which there is a pending control,
           // This is the best time to submit it.
           //
           if (houselights_plugs_pending (plug)) {
               houselog_event ("PLUG", Plugs[plug].name, "RETRY",
                               "%s", Plugs[plug].commanded);
               houselights_plugs_submit (plug);
           }
       }

       Plugs[plug].countdown = MAX_LIFE; // New lease in life.
       Plugs[plug].is_light = (gear >= 0);
   }
}

static void houselights_plugs_discovered
               (void *origin, int status, char *data, int length) {

   const char *provider = (const char *) origin;

   status = echttp_redirected("GET");
   if (!status) {
       echttp_submit (0, 0, houselights_plugs_discovered, origin);
       return;
   }

   if (status != 200) {
       houselog_trace (HOUSE_FAILURE, provider, "HTTP error %d", status);
       return;
   }
   houselights_plugs_discovery (provider, data, length);
}

static void houselights_plugs_scan_server
                (const char *service, void *context, const char *provider) {

    char url[256];

    provider = houselights_plugs_provider_keep (provider);

    snprintf (url, sizeof(url), "%s/status", provider);

    DEBUG ("Attempting discovery at %s\n", url);
    const char *error = echttp_client ("GET", url);
    if (error) {
        houselog_trace (HOUSE_FAILURE, provider, "%s", error);
        return;
    }
    echttp_submit (0, 0, houselights_plugs_discovered, (void *)provider);
}

static void houselights_plugs_prune (time_t now) {

    int i;

    for (i = PlugsCount-1; i >= 0; --i) {
        if (houselights_plugs_pending (i)) continue;
        if (Plugs[i].name) {
            if (--(Plugs[i].countdown) <= 0) {
                 DEBUG ("Plug %s on %s pruned\n", Plugs[i].name, Plugs[i].url);
                 houselog_event
                     ("PLUG", Plugs[i].name, "PRUNE", "FROM %s", Plugs[i].url);
                free(Plugs[i].name);
                Plugs[i].name = 0;
                Plugs[i].url[0] = 0;
            }
        }
    }
    while (PlugsCount > 0 && (!Plugs[PlugsCount-1].name)) PlugsCount -= 1;
}

static void houselights_plugs_controlled
               (void *origin, int status, char *data, int length) {

   LightPlug *plug = Plugs+(int)(0xffffffff & (intptr_t)origin);

   status = echttp_redirected("GET");
   if (!status) {
       echttp_submit (0, 0, houselights_plugs_controlled, origin);
       return;
   }

   // TBD: add an event to record that the command was processed. Too verbose?
   if (status != 200) {
       if (plug->status != 'e') {
           houselog_trace (HOUSE_FAILURE, plug->name, "HTTP code %d", status);
           plug->status = 'e';
       }
       return;
   }
   plug->status = 'i';

   if (!data) return;
   houselights_plugs_discovery (plug->url, data, length);
}

static void houselights_plugs_submit (int plug) {

    static char url[256];

    int pulse = 0;
    time_t now = time(0);
    char encoded[128];

    if (! Plugs[plug].url[0]) {
        houselog_event ("PLUG", Plugs[plug].name, "IGNORED", "NOT DISCOVERED");
        return;
    }

    if (Plugs[plug].deadline > 0)
        pulse = (int) (Plugs[plug].deadline - now);

    echttp_encoding_escape (Plugs[plug].name, encoded, sizeof(encoded));

    snprintf (url, sizeof(url), "%s/set?point=%s&state=%s&pulse=%d",
              Plugs[plug].url,
              encoded,
              Plugs[plug].commanded,
              pulse);
    const char *error = echttp_client ("GET", url);
    if (error) {
        houselog_trace (HOUSE_FAILURE, Plugs[plug].name, "cannot create socket for %s, %s", url, error);
        return;
    }
    DEBUG ("GET %s\n", url);
    echttp_submit (0, 0, houselights_plugs_controlled, (void *)((intptr_t)plug));
    return;
}

static void houselights_plugs_set (const char *name,
                                   const char *state, int pulse, int manual) {

    int plug = houselights_plugs_search (name);
    if (plug < 0) return;

    time_t now = time(0);
    DEBUG ("%ld: Start plug %s for %d seconds (%s)\n",
           (long)now, Plugs[plug].name, pulse, manual?"manual":"scheduled");

    Plugs[plug].requested = now;
    Plugs[plug].commanded = state;
    Plugs[plug].manual = manual;
    if (Plugs[plug].status == 'i') Plugs[plug].status = 'a';

    if (pulse <= 0) {
        Plugs[plug].deadline = 0;
        // Never turn a light on forever. Do not waste energy.
        if (!strcmp (state, "on"))
            Plugs[plug].deadline = now + PLUG_ON_LIMIT;
    } else {
        Plugs[plug].deadline = now + pulse;
    }

    if (manual) { // Scheduled controls are logged by the scheduler
        if (pulse) {
            houselog_event ("PLUG", Plugs[plug].name, "CONTROLLED",
                            "%s FOR %d SECONDS", Plugs[plug].commanded, pulse);
        } else {
            houselog_event ("PLUG", Plugs[plug].name, "CONTROLLED",
                            "%s", Plugs[plug].commanded);
        }
    }
    houselights_plugs_submit (plug);
}

void houselights_plugs_on (const char *name, int pulse, int manual) {

    houselights_plugs_set (name, "on", pulse, manual);
}

void houselights_plugs_off (const char *name, int manual) {

    houselights_plugs_set (name, "off", 0, manual);
}

void houselights_plugs_periodic (time_t now) {

    static time_t starting = 0;
    static time_t latestdiscovery = 0;
    int i;

    if (!now) { // This is a manual reset (force a discovery refresh)
        starting = 0;
        latestdiscovery = 0;
        return;
    }
    if (starting == 0) starting = now;

    if (now >= latestdiscovery + 2) {
        for (i = 0; i < PlugsCount; ++i) {
            if (Plugs[i].url[0] == 0) continue;
            if (houselights_plugs_pending(i)) {
                // Force a discovery ever 2 seconds, but not immediately
                // after the command.
                if (now > Plugs[i].requested) {
                    latestdiscovery = 0;
                    break;
                }
            }
        }
    }

    // Scan every 15s for the first 2 minutes, then slow down to every minute.
    // The fast start is to make the whole network recover fast from
    // an outage, when we do not know in which order the systems start.
    // Later on, there is no need to create more traffic.
    // The exception is when there are control pending: we then need a faster
    // refresh because we expect changes.
    //
    if (now <= latestdiscovery + 15) return;
    if (now <= latestdiscovery + 60 && now >= starting + 120) return;
    latestdiscovery = now;

    DEBUG ("Proceeding with discovery\n");
    housediscovered ("control", 0, houselights_plugs_scan_server);
    houselights_plugs_prune (now);
}

int houselights_plugs_status (char *buffer, int size) {

    int i;
    int cursor = 0;
    const char *prefix = "";

    cursor = snprintf (buffer, size, "\"servers\":[");
    if (cursor >= size) goto overflow;

    for (i = 0; i < ProvidersCount; ++i) {
        cursor += snprintf (buffer+cursor, size-cursor,
                            "%s\"%s\"", prefix, Providers[i]);
        if (cursor >= size) goto overflow;
        prefix = ",";
    }
    cursor += snprintf (buffer+cursor, size-cursor, "]");
    if (cursor >= size) goto overflow;

    cursor += snprintf (buffer+cursor, size-cursor, ",\"plugs\":[");
    if (cursor >= size) goto overflow;
    prefix = "";

    for (i = 0; i < PlugsCount; ++i) {
        char s[512];
        char p[256];

        if (!Plugs[i].name) continue; // Ignore obsolete entries.

        if (Plugs[i].url[0])
            snprintf (s, sizeof(s), ",\"url\":\"%s\"", Plugs[i].url);
        else
            s[0] = 0; // URL is not yet known.

        if (Plugs[i].deadline && Plugs[i].commanded)
            snprintf (p, sizeof(p), ",\"command\":\"%s\",\"expires\":%ld",
                      Plugs[i].commanded, (long)(Plugs[i].deadline));
        else
            p[0] = 0;

        cursor += snprintf (buffer+cursor, size-cursor,
                            "%s{\"name\":\"%s\",\"status\":\"%c\",\"state\":\"%s\",\"light\":%s%s%s}",
                            prefix, Plugs[i].name, Plugs[i].status, Plugs[i].state, Plugs[i].is_light?"true":"false", s, p);
        if (cursor >= size) goto overflow;
        prefix = ",";
    }

    cursor += snprintf (buffer+cursor, size-cursor, "]");
    if (cursor >= size) goto overflow;

    return cursor;

overflow:
    houselog_trace (HOUSE_FAILURE, "BUFFER", "overflow");
    buffer[0] = 0;
    return 0;
}

