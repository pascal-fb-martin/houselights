/* houseslights - A simple home web server for lighting control
 *
 * Copyright 2025, Pascal Martin
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
 * houselights_template.c - Generate HTML on the fly from template files.
 *
 * SYNOPSYS:
 *
 * This module handles pages that include SVG content created using
 * Inkscape. The tool does not embbed the SVG in an HTML page by itself,
 * so you must do this yourself. However any modification to the SVG means
 * the HTML integration is to be done all over again. This module is a
 * runtime solution for automating that process.
 *
 * const char *houselights_template_initialize
 *                 (int argc, const char **argv, const char *rooturi);
 *
 *    Install the templating mechanism.
 */

#include <sys/time.h>

#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <fcntl.h>

#include <echttp.h>
#include <echttp_static.h>

#include "houselog.h"
#include "houseconfig.h"

#include "houselights_template.h"

#define DEBUG if (echttp_isdebug()) printf

static const char *HouseLightsContentRoot = "/var/lib/house/lights";
static const char *HouseLightsWebRoot = "/var/cache/house/lights";

static int HouseLightsContentRootLength = 0;
static int HouseLightsWebRootLength = 0;

static echttp_not_found_handler *HouseLightsTranscodeChain = 0;


static void houselights_template_patch (char *text, const char *prefix) {

    char *attribute = strstr (text, prefix);
    if (!attribute) return;
    attribute += strlen (prefix);
    *(attribute++) = '1';
    *(attribute++) = '0';
    *(attribute++) = '0';
    *(attribute++) = '%';
    if (*attribute != '"') {
       *(attribute++) = '"';
       while (*attribute != '"') *(attribute++) = ' ';
       *attribute = ' ';
    }
}

static void houselights_template_expand (FILE *in, FILE *out) {

   char source[1024];

   while (fgets(source, sizeof(source), in)) {
      char *cursor = source - 1;
      while (*(++cursor) <= ' ') {if (*cursor != ' ' || *cursor != '\t') break;}
      if (cursor[0] == '<' && cursor[1] == '<') {
         *cursor = 0; // Maintain the original indentation.

         // Extract the include file name.
         char *name = cursor + 2;
         cursor = strchr (name, '\n');
         if (cursor) *cursor = 0;

         // Read the include file and expand into the output.
         // Patch the width and height of the "svg" element.
         //
         char fullpath[1048];
         snprintf (fullpath, sizeof(fullpath),
                   "%s/%s", HouseLightsContentRoot, name);
         FILE *include = fopen (fullpath, "r");
         if (include) {
            char included[1024];
            int issvg = 0;
            while (fgets(included, sizeof(included), include)) {
                if (strstr (included, "<svg")) issvg = 1;
                else if (strchr (included, '<')) issvg = 0;
                if (included[0] == '<') { // Skip XML info.
                   if (included[1] == '?' || included[1] == '!') continue;
                }
                if (issvg) {
                    houselights_template_patch (included, "width=\"");
                    houselights_template_patch (included, "height=\"");
                }
                fputs (source, out);
                fputs (included, out);
            }
            fputs ("\n", out);
            fclose (include);
         }
      } else {
         fputs (source, out); // No include to process: write as-is.
      }
   }
}

static int houselights_template_render (const char *filename) {

   int fd;

   if (HouseLightsTranscodeChain) {
       fd = HouseLightsTranscodeChain (filename);
       if (fd >= 0) return fd;
   }

   if (strncmp (filename, HouseLightsWebRoot, HouseLightsWebRootLength)) {
       // Reject any URL that does not point to the cache.
       return -1;
   }

   char fullpath[1024];
   const char *base = filename + HouseLightsWebRootLength;
   snprintf (fullpath, sizeof(fullpath), "%s%s", HouseLightsContentRoot, base);

   if (!strstr (base, ".html")) {
       // Only render to HTML, but support other formats as-is.
       // In that case, we just pretend that the file was found
       // by opening it at its "installed" location. If the file does not
       // exists, open() will fail and a 404 status will be returned.
       snprintf (fullpath, sizeof(fullpath), "%s%s", HouseLightsContentRoot, base);
       return open (fullpath, O_RDONLY);
   }

   FILE *in = 0;
   FILE *out = 0;

   // Build the source name.
   char *sep = strrchr (fullpath, '.');
   if (!sep) return -1;
   sep[4] = 't'; // The source is an ".htmt" file.

   in = fopen (fullpath, "r");
   if (!in) goto failure;

   // Create all the directories listed in the target file's path.
   // This is as brute force as it can get. To publish is not high
   // volume enough to justify spending brain power..
   //
   snprintf (fullpath, sizeof(fullpath), "mkdir -p %s", filename);
   sep = strrchr (fullpath, '/');
   if (sep) {
      *sep = 0;
      system (fullpath);
   }

   out = fopen (filename, "w+");
   if (!out) goto failure;

   houselights_template_expand (in, out);

   fclose (in);
   fclose (out);
   return open (filename, O_RDONLY);

failure:
   if (in) fclose (in);
   if (out) fclose (out);
   return -1;
}

const char *houselights_template_initialize
                (int argc, const char **argv, const char *rooturi) {

    HouseLightsContentRootLength = strlen(HouseLightsContentRoot);
    HouseLightsWebRootLength = strlen(HouseLightsWebRoot);

    echttp_static_route (rooturi, HouseLightsWebRoot);

    HouseLightsTranscodeChain =
        echttp_static_on_not_found (houselights_template_render);

    return 0;
}

